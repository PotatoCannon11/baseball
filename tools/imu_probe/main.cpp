// IMU measurement tool (milestone 1).
//
// This is deliberately a standalone diagnostic executable, not part of the
// game or the sim: its whole job is to answer, empirically, the questions
// the project spec says must be measured rather than assumed:
//   - actual sample rate (not the nominal rate a driver reports)
//   - whether samples arrive individually or batched in bursts
//   - apparent quantization step (a rough resolution estimate)
//   - whether button and IMU events share a timestamp domain
//   - clock offset and drift between the device's sensor timestamp and the
//     host monotonic clock
//   - how two simultaneously-connected devices compare to one
//
// Run it once with one Joy-Con connected, then again with two, and diff the
// printed per-device numbers -- that comparison is the point of running it
// twice; this tool does not attempt to auto-detect "device count changed
// mid-run" as a single report.
//
// UNTESTED WITH REAL HARDWARE as of writing: this was built and smoke-tested
// with zero gamepads attached (the dev machine has none paired). It builds,
// runs, and reports "no gamepads detected" cleanly in that case, which is
// itself useful (that path must not crash), but every number in the report
// below is only as good as its first real run against a Joy-Con.

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "platform/clock.h"
#include "input/device_profile.h"
#include "evdev_probe.h"

namespace {

constexpr int kMaxDevices = 4;
constexpr std::size_t kMaxSamplesPerChannel = 16384;
constexpr std::size_t kMaxButtonSamples = 4096;
constexpr double kDefaultDurationSeconds = 15.0;
// Consecutive samples whose host arrival times are closer together than
// this are considered part of the same delivery "batch" -- i.e. the OS/HID
// layer handed SDL more than one sample in the same wakeup rather than one
// at a time. 1 ms is comfortably smaller than the inter-sample gap of any
// controller IMU we expect to see (all are well under 1 kHz).
constexpr std::uint64_t kBatchGapNs = 1'000'000;

// The sensor channels we know how to interpret. GYRO/ACCEL cover a single
// Joy-Con, a Pro Controller, or a combined Joy-Con pair when SDL merges
// them; the _L/_R variants only appear on a combined pair, one pair of
// channels per physical half. See SDL_sensor.h SDL_SensorType.
constexpr SDL_SensorType kTrackedSensorTypes[] = {
    SDL_SENSOR_GYRO,   SDL_SENSOR_ACCEL,
    SDL_SENSOR_GYRO_L, SDL_SENSOR_ACCEL_L,
    SDL_SENSOR_GYRO_R, SDL_SENSOR_ACCEL_R,
};
constexpr std::size_t kNumTrackedSensorTypes =
    sizeof(kTrackedSensorTypes) / sizeof(kTrackedSensorTypes[0]);

const char* sensor_type_name(SDL_SensorType t) {
    switch (t) {
        case SDL_SENSOR_GYRO: return "GYRO";
        case SDL_SENSOR_ACCEL: return "ACCEL";
        case SDL_SENSOR_GYRO_L: return "GYRO_L";
        case SDL_SENSOR_ACCEL_L: return "ACCEL_L";
        case SDL_SENSOR_GYRO_R: return "GYRO_R";
        case SDL_SENSOR_ACCEL_R: return "ACCEL_R";
        default: return "UNKNOWN";
    }
}

const char* gamepad_type_name(SDL_GamepadType t) {
    switch (t) {
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO: return "Switch Pro Controller";
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT: return "Joy-Con (L), standalone";
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT: return "Joy-Con (R), standalone";
        case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return "Joy-Con pair, combined";
        case SDL_GAMEPAD_TYPE_STANDARD: return "Standard gamepad";
        default: return "Other/unknown";
    }
}

struct SensorSample {
    std::uint64_t host_recv_ns = 0;     // platform::monotonic_now_ns() when we drained this event
    std::uint64_t event_ts_ns = 0;      // event.gsensor.timestamp: SDL_GetTicksNS() domain
    std::uint64_t sensor_ts_ns = 0;     // event.gsensor.sensor_timestamp: device/driver domain
    float data[3] = {0, 0, 0};
};

struct ButtonSample {
    std::uint64_t host_recv_ns = 0;
    std::uint64_t event_ts_ns = 0;
    Uint8 button = 0;
    bool down = false;
};

struct SensorChannelLog {
    bool enabled = false;
    SDL_SensorType type = SDL_SENSOR_INVALID;
    float nominal_rate_hz = 0.0f;
    std::size_t count = 0;
    SensorSample samples[kMaxSamplesPerChannel];
};

struct DeviceLog {
    bool active = false;
    SDL_JoystickID id = 0;
    SDL_Gamepad* gamepad = nullptr;
    SDL_GamepadType type = SDL_GAMEPAD_TYPE_UNKNOWN;
    char name[128] = {};
    Uint16 vendor = 0;
    Uint16 product = 0;
    bool disconnected_mid_run = false;
    std::uint64_t disconnect_host_ns = 0;

    SensorChannelLog channels[kNumTrackedSensorTypes];

    std::size_t button_count = 0;
    ButtonSample buttons[kMaxButtonSamples];
};

// Global, not stack-local: with 4 devices * 6 channels * 16384 samples this
// struct is several MB, too large to safely put on a thread stack.
DeviceLog g_devices[kMaxDevices];

DeviceLog* find_device(SDL_JoystickID id) {
    for (auto& d : g_devices) {
        if (d.active && d.id == id) return &d;
    }
    return nullptr;
}

SensorChannelLog* find_channel(DeviceLog& d, SDL_SensorType type) {
    for (auto& c : d.channels) {
        if (c.enabled && c.type == type) return &c;
    }
    return nullptr;
}

struct IntervalStats {
    double mean_ns = 0.0;
    double stddev_ns = 0.0;
    double min_ns = 0.0;
    double max_ns = 0.0;
    std::uint64_t non_monotonic_count = 0;  // delta <= 0
    std::size_t n = 0;
};

IntervalStats compute_intervals(const std::uint64_t* ts, std::size_t count) {
    IntervalStats s;
    if (count < 2) return s;
    double sum = 0.0;
    double sum_sq = 0.0;
    s.min_ns = 1e300;
    s.max_ns = -1e300;
    for (std::size_t i = 1; i < count; ++i) {
        const double delta = static_cast<double>(ts[i]) - static_cast<double>(ts[i - 1]);
        if (delta <= 0.0) {
            s.non_monotonic_count++;
            continue;
        }
        sum += delta;
        sum_sq += delta * delta;
        if (delta < s.min_ns) s.min_ns = delta;
        if (delta > s.max_ns) s.max_ns = delta;
        s.n++;
    }
    if (s.n == 0) return s;
    s.mean_ns = sum / static_cast<double>(s.n);
    const double variance = sum_sq / static_cast<double>(s.n) - s.mean_ns * s.mean_ns;
    s.stddev_ns = variance > 0.0 ? std::sqrt(variance) : 0.0;
    return s;
}

// Groups samples by host arrival burst and returns {mean_batch_size, max_batch_size}.
void compute_batching(const SensorSample* samples, std::size_t count, double* out_mean,
                       std::size_t* out_max) {
    *out_mean = 0.0;
    *out_max = 0;
    if (count == 0) return;
    std::size_t num_batches = 0;
    std::size_t batch_start = 0;
    for (std::size_t i = 1; i <= count; ++i) {
        const bool end_of_batch =
            (i == count) || (samples[i].host_recv_ns - samples[i - 1].host_recv_ns > kBatchGapNs);
        if (end_of_batch) {
            const std::size_t batch_size = i - batch_start;
            num_batches++;
            if (batch_size > *out_max) *out_max = batch_size;
            batch_start = i;
        }
    }
    *out_mean = static_cast<double>(count) / static_cast<double>(num_batches);
}

// Offset between the two timestamp domains (event.timestamp - sensor_timestamp)
// and its linear drift over the run, via a simple least-squares fit against
// elapsed time. A near-zero, near-constant offset means the two domains are
// effectively the same clock; a growing offset means they tick at
// measurably different rates (drift); a noisy, large, or non-monotonic
// offset means sensor_timestamp is not comparable to host time at all.
struct OffsetStats {
    double mean_ns = 0.0;
    double stddev_ns = 0.0;
    double drift_ns_per_s = 0.0;
    std::size_t n = 0;
};

OffsetStats compute_offset_and_drift(const SensorSample* samples, std::size_t count) {
    OffsetStats s;
    if (count == 0) return s;
    s.n = count;

    double sum = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        sum += static_cast<double>(samples[i].event_ts_ns) - static_cast<double>(samples[i].sensor_ts_ns);
    }
    s.mean_ns = sum / static_cast<double>(count);

    double sum_sq_dev = 0.0;
    // Least-squares slope of offset(t) vs t, t measured from the first sample's
    // event timestamp (host domain, always monotonic by SDL's construction).
    const double t0 = static_cast<double>(samples[0].event_ts_ns);
    double sum_t = 0.0, sum_tt = 0.0, sum_o = 0.0, sum_to = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double offset =
            static_cast<double>(samples[i].event_ts_ns) - static_cast<double>(samples[i].sensor_ts_ns);
        sum_sq_dev += (offset - s.mean_ns) * (offset - s.mean_ns);
        const double t = static_cast<double>(samples[i].event_ts_ns) - t0;
        sum_t += t;
        sum_tt += t * t;
        sum_o += offset;
        sum_to += t * offset;
    }
    s.stddev_ns = std::sqrt(sum_sq_dev / static_cast<double>(count));

    const double n = static_cast<double>(count);
    const double denom = n * sum_tt - sum_t * sum_t;
    if (std::fabs(denom) > 1e-9) {
        // slope in (offset ns)/(time ns) -> convert to ns/s.
        const double slope_ns_per_ns = (n * sum_to - sum_t * sum_o) / denom;
        s.drift_ns_per_s = slope_ns_per_ns * 1e9;
    }
    return s;
}

// Minimum nonzero |delta| between consecutive raw readings on each axis.
// Only meaningful as a resolution estimate if the device was held still;
// this tool has no way to know that from software, so the caller must say
// so in the report.
void estimate_resolution(const SensorSample* samples, std::size_t count, float out_min_step[3]) {
    for (int axis = 0; axis < 3; ++axis) out_min_step[axis] = -1.0f;
    if (count < 2) return;
    for (std::size_t i = 1; i < count; ++i) {
        for (int axis = 0; axis < 3; ++axis) {
            const float d = std::fabs(samples[i].data[axis] - samples[i - 1].data[axis]);
            if (d > 0.0f && (out_min_step[axis] < 0.0f || d < out_min_step[axis])) {
                out_min_step[axis] = d;
            }
        }
    }
}

void report_channel(const DeviceLog& dev, const SensorChannelLog& ch) {
    if (!ch.enabled) return;
    std::printf("  [%s] nominal=%.1f Hz, samples=%zu\n", sensor_type_name(ch.type),
                ch.nominal_rate_hz, ch.count);
    if (ch.count < 2) {
        std::printf("      (not enough samples to compute rate)\n");
        return;
    }

    // Build plain timestamp arrays for the interval helper.
    static std::uint64_t sensor_ts[kMaxSamplesPerChannel];
    static std::uint64_t host_ts[kMaxSamplesPerChannel];
    for (std::size_t i = 0; i < ch.count; ++i) {
        sensor_ts[i] = ch.samples[i].sensor_ts_ns;
        host_ts[i] = ch.samples[i].host_recv_ns;
    }

    const IntervalStats sensor_iv = compute_intervals(sensor_ts, ch.count);
    const IntervalStats host_iv = compute_intervals(host_ts, ch.count);

    if (sensor_iv.n > 0) {
        std::printf("      measured rate (from sensor_timestamp): %.1f Hz  (mean interval %.3f ms, jitter stddev %.3f ms, min %.3f ms, max %.3f ms)\n",
                    1e9 / sensor_iv.mean_ns, sensor_iv.mean_ns / 1e6, sensor_iv.stddev_ns / 1e6,
                    sensor_iv.min_ns / 1e6, sensor_iv.max_ns / 1e6);
    }
    if (sensor_iv.non_monotonic_count > 0) {
        std::printf("      WARNING: %llu non-positive sensor_timestamp deltas (clock is not strictly increasing)\n",
                    static_cast<unsigned long long>(sensor_iv.non_monotonic_count));
    }
    if (host_iv.n > 0) {
        std::printf("      measured rate (from host arrival time):  %.1f Hz  (mean interval %.3f ms, jitter stddev %.3f ms)\n",
                    1e9 / host_iv.mean_ns, host_iv.mean_ns / 1e6, host_iv.stddev_ns / 1e6);
    }

    double mean_batch = 0.0;
    std::size_t max_batch = 0;
    compute_batching(ch.samples, ch.count, &mean_batch, &max_batch);
    std::printf("      batching: mean %.2f samples/wakeup, max %zu samples in one wakeup (wakeups grouped within %.1f ms)\n",
                mean_batch, max_batch, kBatchGapNs / 1e6);

    const OffsetStats off = compute_offset_and_drift(ch.samples, ch.count);
    std::printf("      event.timestamp - sensor_timestamp: mean %.3f ms, stddev %.3f ms, drift %.4f ms/s\n",
                off.mean_ns / 1e6, off.stddev_ns / 1e6, off.drift_ns_per_s / 1e6);
    std::printf("      (magnitude may just reflect different epochs between the two clocks; stddev/drift are the meaningful parts)\n");

    float min_step[3];
    estimate_resolution(ch.samples, ch.count, min_step);
    std::printf("      apparent quantization step per axis (ONLY valid if device was held still): x=%.6f y=%.6f z=%.6f (SDL units: rad/s for gyro, m/s^2 for accel)\n",
                min_step[0], min_step[1], min_step[2]);

    const bool is_gyro = (ch.type == SDL_SENSOR_GYRO || ch.type == SDL_SENSOR_GYRO_L || ch.type == SDL_SENSOR_GYRO_R);
    const DeviceProfile profile = get_device_profile(dev.type);
    if (profile.ranges_are_documented_not_measured || profile.gyro_range_dps > 0.0f) {
        const float limit = is_gyro ? profile.gyro_range_dps * (3.14159265f / 180.0f)
                                     : profile.accel_range_g * 9.80665f;
        std::size_t clip_count = 0;
        for (std::size_t i = 0; i < ch.count; ++i) {
            for (int axis = 0; axis < 3; ++axis) {
                if (std::fabs(ch.samples[i].data[axis]) >= 0.98f * limit) {
                    clip_count++;
                    break;
                }
            }
        }
        std::printf("      clip check against documented range (%s, %.0f dps / %.1f g): %zu / %zu samples >= 98%% of range\n",
                    profile.label, profile.gyro_range_dps, profile.accel_range_g, clip_count, ch.count);
    } else {
        std::printf("      clip check skipped: no documented range for this device type\n");
    }
}

void report_device(const DeviceLog& dev) {
    std::printf("\nDevice: %s (SDL type: %s, vendor=0x%04x product=0x%04x, joystick id=%u)\n",
                dev.name, gamepad_type_name(dev.type), dev.vendor, dev.product,
                static_cast<unsigned>(dev.id));
    if (dev.disconnected_mid_run) {
        std::printf("  NOTE: disconnected mid-run at t=%.3f s into capture\n",
                    dev.disconnect_host_ns / 1e9);
    }
    for (const auto& ch : dev.channels) {
        report_channel(dev, ch);
    }

    std::printf("  Buttons observed: %zu events\n", dev.button_count);
    // Report the smallest observed gap between a button event and the
    // nearest sensor event from the SAME device, across all its enabled
    // channels, as a proxy for "do buttons and IMU samples share a report".
    if (dev.button_count > 0) {
        double best_gap_ns = 1e300;
        for (std::size_t bi = 0; bi < dev.button_count; ++bi) {
            const std::uint64_t bt = dev.buttons[bi].event_ts_ns;
            for (const auto& ch : dev.channels) {
                if (!ch.enabled) continue;
                for (std::size_t si = 0; si < ch.count; ++si) {
                    const double gap = std::fabs(static_cast<double>(ch.samples[si].event_ts_ns) -
                                                  static_cast<double>(bt));
                    if (gap < best_gap_ns) best_gap_ns = gap;
                }
            }
        }
        if (best_gap_ns < 1e299) {
            std::printf("  closest button-event-timestamp to sensor-event-timestamp gap seen: %.3f ms\n",
                        best_gap_ns / 1e6);
        }
    }
}

double parse_duration_arg(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            return std::atof(argv[i + 1]);
        }
    }
    return kDefaultDurationSeconds;
}

bool wants_evdev_backend(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--backend") == 0 && i + 1 < argc && std::strcmp(argv[i + 1], "evdev") == 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    const double duration_seconds = parse_duration_arg(argc, argv);

    if (wants_evdev_backend(argc, argv)) {
#if defined(BASEBALL_HAVE_EVDEV)
        return run_evdev_probe(duration_seconds);
#else
        std::fprintf(stderr,
                      "--backend evdev requested, but this build has no libevdev-dev / is not "
                      "Linux. The evdev backend is optional; omit --backend to use the primary "
                      "SDL3 path.\n");
        return 1;
#endif
    }

    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // Print the hints that determine single-vs-combined Joy-Con exposure, so
    // the report is self-describing about which mode was active.
    const char* combine_hint = SDL_GetHint(SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS);
    std::printf("SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS = %s (SDL default: \"1\", i.e. combine)\n",
                combine_hint ? combine_hint : "(unset, using default)");

    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    std::printf("Gamepads detected: %d\n", count);

    if (count == 0) {
        std::printf(
            "No gamepads detected. This is the expected result on a machine with no\n"
            "Joy-Con/Pro Controller paired (true for this dev environment). Connect a\n"
            "controller and re-run to get real measurements.\n");
        if (ids) SDL_free(ids);
        SDL_Quit();
        return 0;
    }

    int opened = 0;
    for (int i = 0; i < count && opened < kMaxDevices; ++i) {
        SDL_Gamepad* gp = SDL_OpenGamepad(ids[i]);
        if (!gp) {
            std::fprintf(stderr, "Failed to open gamepad %d: %s\n", ids[i], SDL_GetError());
            continue;
        }
        DeviceLog& dev = g_devices[opened++];
        dev.active = true;
        dev.id = ids[i];
        dev.gamepad = gp;
        dev.type = SDL_GetGamepadType(gp);
        const char* name = SDL_GetGamepadName(gp);
        std::snprintf(dev.name, sizeof(dev.name), "%s", name ? name : "(unnamed)");
        dev.vendor = SDL_GetGamepadVendor(gp);
        dev.product = SDL_GetGamepadProduct(gp);

        for (std::size_t c = 0; c < kNumTrackedSensorTypes; ++c) {
            const SDL_SensorType t = kTrackedSensorTypes[c];
            if (!SDL_GamepadHasSensor(gp, t)) continue;
            if (!SDL_SetGamepadSensorEnabled(gp, t, true)) {
                std::fprintf(stderr, "  Warning: failed to enable sensor %s: %s\n",
                             sensor_type_name(t), SDL_GetError());
                continue;
            }
            dev.channels[c].enabled = true;
            dev.channels[c].type = t;
            dev.channels[c].nominal_rate_hz = SDL_GetGamepadSensorDataRate(gp, t);
        }

        std::printf("Opened: %s (%s)\n", dev.name, gamepad_type_name(dev.type));
    }
    if (ids) SDL_free(ids);

    if (opened == kMaxDevices && count > kMaxDevices) {
        std::printf("Note: %d gamepads detected but only the first %d are tracked (kMaxDevices).\n",
                     count, kMaxDevices);
    }

    std::printf("\nCapturing for %.1f s. Swing/rotate the controller(s) now to exercise the full range;\n"
                "hold still for a few seconds too, since the resolution estimate only means anything\n"
                "when the device isn't moving.\n\n",
                duration_seconds);

    const std::uint64_t start_ns = platform::monotonic_now_ns();
    const std::uint64_t end_ns = start_ns + static_cast<std::uint64_t>(duration_seconds * 1e9);

    while (platform::monotonic_now_ns() < end_ns) {
        SDL_Event ev;
        // Block briefly rather than busy-spin; SDL requires event pumping on
        // the main thread on some platforms (notably macOS), which is fine
        // here since this tool has no other work to do between pumps.
        if (!SDL_WaitEventTimeout(&ev, 20)) {
            continue;
        }
        do {
            const std::uint64_t host_now = platform::monotonic_now_ns();
            if (ev.type == SDL_EVENT_GAMEPAD_SENSOR_UPDATE) {
                DeviceLog* dev = find_device(ev.gsensor.which);
                if (dev) {
                    SensorChannelLog* ch =
                        find_channel(*dev, static_cast<SDL_SensorType>(ev.gsensor.sensor));
                    if (ch && ch->count < kMaxSamplesPerChannel) {
                        SensorSample& s = ch->samples[ch->count++];
                        s.host_recv_ns = host_now;
                        s.event_ts_ns = ev.gsensor.timestamp;
                        s.sensor_ts_ns = ev.gsensor.sensor_timestamp;
                        s.data[0] = ev.gsensor.data[0];
                        s.data[1] = ev.gsensor.data[1];
                        s.data[2] = ev.gsensor.data[2];
                    }
                }
            } else if (ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
                       ev.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
                DeviceLog* dev = find_device(ev.gbutton.which);
                if (dev && dev->button_count < kMaxButtonSamples) {
                    ButtonSample& b = dev->buttons[dev->button_count++];
                    b.host_recv_ns = host_now;
                    b.event_ts_ns = ev.gbutton.timestamp;
                    b.button = ev.gbutton.button;
                    b.down = ev.gbutton.down;
                }
            } else if (ev.type == SDL_EVENT_GAMEPAD_REMOVED) {
                DeviceLog* dev = find_device(ev.gdevice.which);
                if (dev) {
                    dev->disconnected_mid_run = true;
                    dev->disconnect_host_ns = host_now - start_ns;
                }
            }
        } while (SDL_PollEvent(&ev));
    }

    std::printf("\n=== Report ===\n");
    for (int i = 0; i < opened; ++i) {
        report_device(g_devices[i]);
    }

    if (opened >= 2) {
        std::printf(
            "\n%d devices were connected for this run. Compare the per-device measured\n"
            "rate/jitter numbers above against a run with only 1 device connected to see\n"
            "whether two devices degrade sample rate, latency, or drop reports.\n",
            opened);
    } else {
        std::printf(
            "\nOnly 1 device was connected for this run. Re-run with a second controller\n"
            "connected and compare the numbers above to check for degradation with two.\n");
    }

    for (int i = 0; i < opened; ++i) {
        if (g_devices[i].gamepad) SDL_CloseGamepad(g_devices[i].gamepad);
    }
    SDL_Quit();
    return 0;
}
