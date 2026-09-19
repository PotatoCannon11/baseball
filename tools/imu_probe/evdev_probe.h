#pragma once

// Optional Linux-only backend (spec: "Optional Linux backend: evdev via
// libevdev/epoll"). Only compiled when BASEBALL_HAVE_EVDEV is defined,
// which the imu_probe CMakeLists.txt only sets when libevdev-dev was found.
#if defined(BASEBALL_HAVE_EVDEV)

// Scans /dev/input/event* for Joy-Con/Pro-Controller-looking nodes via
// libevdev, reports their raw capabilities as the kernel driver describes
// them, and if any expose axis/button events, captures for
// duration_seconds and reports the same kind of rate/jitter/batching
// numbers as the SDL backend, using each event's kernel-timestamped
// struct timeval as the source clock.
//
// Returns a process exit code (0 on success, including "found nothing").
int run_evdev_probe(double duration_seconds);

#endif  // BASEBALL_HAVE_EVDEV
