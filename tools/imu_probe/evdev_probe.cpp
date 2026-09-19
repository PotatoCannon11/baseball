#include "evdev_probe.h"

#if defined(BASEBALL_HAVE_EVDEV)

#include <libevdev/libevdev.h>
#include <linux/input.h>

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "platform/clock.h"

namespace {

constexpr int kMaxMatchedDevices = 4;
constexpr std::size_t kMaxReports = 16384;

std::uint64_t timeval_to_ns(const struct timeval& tv) {
    return static_cast<std::uint64_t>(tv.tv_sec) * 1'000'000'000ull +
           static_cast<std::uint64_t>(tv.tv_usec) * 1'000ull;
}

bool name_looks_like_joycon_or_pro(const char* name) {
    if (!name) return false;
    // Case-insensitive substring check against the two strings the
    // in-kernel hid-nintendo driver is known to report. Anything else is
    // deliberately left unmatched rather than guessed at.
    char lower[256];
    std::size_t n = std::strlen(name);
    if (n >= sizeof(lower)) n = sizeof(lower) - 1;
    for (std::size_t i = 0; i < n; ++i) lower[i] = static_cast<char>(std::tolower(name[i]));
    lower[n] = '\0';
    return std::strstr(lower, "joy-con") != nullptr || std::strstr(lower, "joycon") != nullptr ||
           std::strstr(lower, "pro controller") != nullptr;
}

struct MatchedDevice {
    int fd = -1;
    struct libevdev* dev = nullptr;
    char path[300] = {};
    bool monotonic_clock_set = false;

    std::size_t report_count = 0;
    std::uint64_t report_device_ns[kMaxReports];
    std::uint64_t report_host_ns[kMaxReports];
    std::uint32_t fields_since_last_report = 0;
    std::uint32_t fields_per_report[kMaxReports];
};

MatchedDevice g_devices[kMaxMatchedDevices];

void print_abs_capabilities(struct libevdev* dev) {
    if (!libevdev_has_event_type(dev, EV_ABS)) {
        std::printf("    no EV_ABS axes\n");
        return;
    }
    for (unsigned code = 0; code < ABS_MAX; ++code) {
        if (!libevdev_has_event_code(dev, EV_ABS, code)) continue;
        const struct input_absinfo* info = libevdev_get_abs_info(dev, code);
        if (!info) continue;
        std::printf("    ABS code %u: min=%d max=%d fuzz=%d flat=%d resolution=%d (units/mm or units/rad per kernel driver, driver-defined)\n",
                    code, info->minimum, info->maximum, info->fuzz, info->flat, info->resolution);
    }
}

}  // namespace

int run_evdev_probe(double duration_seconds) {
    DIR* dir = opendir("/dev/input");
    if (!dir) {
        std::fprintf(stderr, "Could not open /dev/input: %s\n", std::strerror(errno));
        return 1;
    }

    int matched = 0;
    int scanned = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr && matched < kMaxMatchedDevices) {
        if (std::strncmp(entry->d_name, "event", 5) != 0) continue;
        scanned++;

        char path[300];
        std::snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            if (errno == EACCES) {
                std::printf(
                    "%s: permission denied. On most distros /dev/input/event* is owned by\n"
                    "the 'input' group; add your user to it (sudo usermod -aG input $USER,\n"
                    "then re-login) rather than running this as root.\n",
                    path);
            }
            continue;
        }

        struct libevdev* dev = nullptr;
        if (libevdev_new_from_fd(fd, &dev) < 0) {
            close(fd);
            continue;
        }

        const char* name = libevdev_get_name(dev);
        if (!name_looks_like_joycon_or_pro(name)) {
            libevdev_free(dev);
            close(fd);
            continue;
        }

        std::printf("Matched evdev node %s: \"%s\"\n", path, name ? name : "(unnamed)");

        // Request the monotonic clock domain for event timestamps so they're
        // directly comparable to platform::monotonic_now_ns() (steady_clock,
        // which is CLOCK_MONOTONIC on Linux). Without this, evdev defaults to
        // a realtime-ish clock that can jump under NTP adjustment.
        clockid_t clock_id = CLOCK_MONOTONIC;
        const bool clock_set = (ioctl(fd, EVIOCSCLOCKID, &clock_id) == 0);
        std::printf("  EVIOCSCLOCKID(CLOCK_MONOTONIC): %s\n", clock_set ? "ok" : std::strerror(errno));

        print_abs_capabilities(dev);
        if (libevdev_has_event_type(dev, EV_KEY)) {
            std::printf("    has EV_KEY (buttons)\n");
        }

        MatchedDevice& m = g_devices[matched++];
        m.fd = fd;
        m.dev = dev;
        m.monotonic_clock_set = clock_set;
        std::snprintf(m.path, sizeof(m.path), "%s", path);
    }
    closedir(dir);

    std::printf("Scanned %d /dev/input/event* nodes, matched %d Joy-Con/Pro-Controller-like devices.\n",
                scanned, matched);
    if (matched == 0) {
        std::printf(
            "No matches. Either nothing is paired, the in-kernel hid-nintendo driver isn't\n"
            "loaded/available on this system, or permissions blocked every node (see above).\n"
            "This evdev path is optional; the SDL3 path is primary and does not need it.\n");
        return 0;
    }

    std::printf("\nCapturing for %.1f s via evdev...\n", duration_seconds);

    struct pollfd fds[kMaxMatchedDevices];
    for (int i = 0; i < matched; ++i) {
        fds[i].fd = g_devices[i].fd;
        fds[i].events = POLLIN;
    }

    const std::uint64_t start_ns = platform::monotonic_now_ns();
    const std::uint64_t end_ns = start_ns + static_cast<std::uint64_t>(duration_seconds * 1e9);

    while (platform::monotonic_now_ns() < end_ns) {
        const int timeout_ms = 20;
        const int rc = poll(fds, static_cast<nfds_t>(matched), timeout_ms);
        if (rc <= 0) continue;

        for (int i = 0; i < matched; ++i) {
            if (!(fds[i].revents & POLLIN)) continue;
            MatchedDevice& m = g_devices[i];
            struct input_event ev;
            int read_rc;
            while ((read_rc = libevdev_next_event(m.dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) == LIBEVDEV_READ_STATUS_SUCCESS ||
                   read_rc == LIBEVDEV_READ_STATUS_SYNC) {
                const std::uint64_t host_now = platform::monotonic_now_ns();
                if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
                    if (m.report_count < kMaxReports) {
                        m.report_device_ns[m.report_count] = timeval_to_ns(ev.time);
                        m.report_host_ns[m.report_count] = host_now;
                        m.fields_per_report[m.report_count] = m.fields_since_last_report;
                        m.report_count++;
                    }
                    m.fields_since_last_report = 0;
                } else if (ev.type == EV_ABS || ev.type == EV_KEY) {
                    m.fields_since_last_report++;
                }
            }
        }
    }

    std::printf("\n=== evdev report ===\n");
    for (int i = 0; i < matched; ++i) {
        MatchedDevice& m = g_devices[i];
        std::printf("\n%s (monotonic clock: %s)\n", m.path, m.monotonic_clock_set ? "yes" : "NO (see warning above)");
        std::printf("  SYN_REPORT count: %zu\n", m.report_count);
        if (m.report_count >= 2) {
            double sum = 0.0, sum_sq = 0.0, min_iv = 1e300, max_iv = -1e300;
            std::size_t n = 0;
            for (std::size_t r = 1; r < m.report_count; ++r) {
                const double d = static_cast<double>(m.report_device_ns[r]) -
                                  static_cast<double>(m.report_device_ns[r - 1]);
                if (d <= 0.0) continue;
                sum += d;
                sum_sq += d * d;
                if (d < min_iv) min_iv = d;
                if (d > max_iv) max_iv = d;
                n++;
            }
            if (n > 0) {
                const double mean = sum / static_cast<double>(n);
                const double variance = sum_sq / static_cast<double>(n) - mean * mean;
                const double stddev = variance > 0.0 ? std::sqrt(variance) : 0.0;
                std::printf("  measured report rate: %.1f Hz (mean interval %.3f ms, jitter stddev %.3f ms, min %.3f ms, max %.3f ms)\n",
                            1e9 / mean, mean / 1e6, stddev / 1e6, min_iv / 1e6, max_iv / 1e6);
            }
            double sum_fields = 0.0;
            std::uint32_t max_fields = 0;
            for (std::size_t r = 0; r < m.report_count; ++r) {
                sum_fields += m.fields_per_report[r];
                if (m.fields_per_report[r] > max_fields) max_fields = m.fields_per_report[r];
            }
            std::printf("  fields (ABS+KEY events) per report: mean %.2f, max %u\n",
                        sum_fields / static_cast<double>(m.report_count), max_fields);

            double off_sum = 0.0;
            for (std::size_t r = 0; r < m.report_count; ++r) {
                off_sum += static_cast<double>(m.report_host_ns[r]) - static_cast<double>(m.report_device_ns[r]);
            }
            std::printf("  host_recv - device_timestamp: mean %.3f ms (expect near-constant if clock is truly monotonic-matched)\n",
                        (off_sum / static_cast<double>(m.report_count)) / 1e6);
        } else {
            std::printf("  not enough SYN_REPORT events captured to compute rate\n");
        }
    }

    for (int i = 0; i < matched; ++i) {
        libevdev_free(g_devices[i].dev);
        close(g_devices[i].fd);
    }
    return 0;
}

#endif  // BASEBALL_HAVE_EVDEV
