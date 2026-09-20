# Physics-first motion-controlled baseball sandbox
[![CI](https://github.com/PotatoCannon11/baseball/actions/workflows/ci.yml/badge.svg)](https://github.com/PotatoCannon11/baseball/actions/workflows/ci.yml)

Full design spec: [docs/ORIGINAL_PROMPT.md](docs/ORIGINAL_PROMPT.md). Read
that before making architectural decisions -- it's the source of truth for
scope, priorities, and the rules (determinism, memory budget, sim isolation)
that later milestones depend on.

Current status and a running handoff log for agents on other platforms:
[docs/PROGRESS.md](docs/PROGRESS.md). Check it before starting work, and
append to its Log section when you finish a session.

## Status: Milestone 7 (local two-player on one screen)

Milestone 1 (platform layer, allocation counter, IMU measurement tool)
built, ran, and passed its checks first -- see git history for that state
if needed. Current state adds the full input layer:

- `src/common/`: header-only, dependency-free POD types shared by the
  input layer and (eventually) the sim. `PlayerInput` (fixed-size,
  versioned, quantized orientation/angular-velocity/stick/buttons) and
  `RingBuffer<T, Capacity>` (fixed-capacity, power-of-two, no allocation).
- `src/input/`: the input layer.
  - `ImuSource` interface, `device_profile.h` (gyro/accel range table,
    flagged as documented-not-measured), per-device `ImuRing`/`ButtonRing`.
  - `GyroBiasCalibrator`, `ClipRecoveryWindow<N>` (quadratic fit over
    unclipped neighbors), `MadgwickFilter` (gyro+accel orientation, cites
    the Madgwick 2010 algorithm), `bat_model.h` (`|omega| * r`).
  - `JoyconSource` (real SDL3 gamepad+sensor devices) and
    `MouseKeyboardSource` (dev-only stand-ins: mouse-drag and a keyboard
    mapping), both behind the same `ImuSource` interface.
  - `SdlInputHub`: owns the one process-wide `SDL_PollEvent` loop (SDL's
    event queue is global, so nothing else may call it) and the
    `BindingTable` mapping devices to player slots.
  - `PlayerInputRecorder`/`PlayerInputReplayer`: record/replay a
    `PlayerInput` stream to/from a file, bit-for-bit.
  - `MotionPipeline`: composes clip recovery -> bias subtraction ->
    accelerometer-gated orientation filter -> bat model over one source's
    ring buffer.
- `tools/barrel_speed`: the milestone-2 acceptance-test tool -- live
  terminal barrel-speed readout, `--backend mouse|keyboard` for dev
  testing without a controller, `--record <file>` to also write a
  `PlayerInput` stream.
- 12 tests, including one (`joycon_source_sdl_integration`) that attaches
  an SDL3 *virtual joystick* (a software-only fake device SDL treats like
  real hardware) and injects synthetic sensor data through the real SDL
  event queue -- this validates the actual SDL dispatch path without
  needing a physical controller.

Verified **only on Linux** (this dev machine). macOS and Windows are wired
into CI but not yet confirmed by an actual run.

**What still needs a real Joy-Con, and is not yet verified:**
- The actual barrel-speed number `tools/barrel_speed` reports on a real
  hard swing (spec expects ~25-40 m/s). Everything upstream of that number
  is tested with synthetic data (`motion_pipeline_test.cpp`), but synthetic
  data can't validate real sensor behavior.
- Left/right Joy-Con axis mapping (`device_profile.h`'s
  `axis_mapping_verified = false`) -- whether SDL already normalizes L vs
  R orientation or leaves it mirrored is unverified either way.
- The gyro/accel clip range table (documented from public reverse-
  engineering references, not an official spec) -- needs an actual
  clipped swing to cross-check against `imu_probe`'s clip counter.
- The bat model's arm-length constant (`kDefaultArmLengthMeters = 0.75`)
  is a placeholder; the spec's "calibration swing sets the virtual arm
  length" needs a real device in hand to design a sensible calibration
  procedure against.

## Milestones 3-6 (sim, rendering, physics, throwing, CPU players)

Built on top of the milestone 1/2 layers above, added since this section
was last written (the paragraphs above describe milestone 1/2 exactly as
they shipped; nothing there has changed):

- `src/sim/`: the deterministic simulation core. Depends on nothing but
  `common` + the C++ standard library (enforced by never linking anything
  else into the `sim` CMake target) -- no SDL, no platform, no input.
  `SimState` (tick, ball, bat, pitcher arm, RNG) is one trivially-copyable
  POD; `step(prev, inputs, config)` is a pure function. RK4 flight
  (gravity, Reynolds-dependent drag, Magnus, spin decay, added mass/
  buoyancy), continuous collision (bisection-refined swept-sphere vs. a
  bat capsule chain built from the same profile data the renderer laths),
  Hunt-Crossley compliant contact at microsecond substeps, ground contact
  (restitution + sliding-to-rolling friction + rolling resistance,
  grass/turf/dirt presets), a pitching machine, and throwing (kinematic
  arm model, button-up release timing, 4 grip presets with a
  controller-angular-velocity-to-spin matrix, finger-offset spin, and the
  mass/size effort-scaling formula). `xoshiro256**` RNG, FNV-1a state
  hashing, a 64-tick snapshot ring.
- `src/render/`: hand-written minimal OpenGL 4.1 core loader (no
  glad/Python tooling available in this environment), icosphere/lathe/
  ground meshes, one shadow map, a debug text overlay.
- `src/cpu/`: CPU pitcher and batter AI plus at-bat classification.
  Depends only on `sim` + `common` -- a CPU player is just another
  `common::PlayerInput` producer, exactly like a human input source, so
  `sim::step` never knows the difference. `cpu::PitcherAi` solves the arm
  angular velocity that gives a desired release velocity (with ballistic
  targeting that compensates for gravity drop over an 18 m throw) and
  scripts a windup-then-release button sequence; `cpu::BatterAi` predicts
  the pitch's plate crossing from a deliberately imperfect straight-line
  read and scripts a timed swing arc with Gaussian timing error.
  `cpu::at_bat` classifies each pitch (called ball/strike, swinging
  strike, foul via a 90-degree fair-territory wedge, or fair with exit
  velocity/launch angle/carry distance).
- `app/game`: ties sim + render + input together into the actual
  executable (mouse-drag dev input drives the batter role; no CPU
  integration into the interactive game loop yet -- CPU players currently
  only run through the headless tool below).
- `tools/sim_headless`: scripted/benchmarked sim runs, no window.
- `tools/cpu_vs_cpu`: headless CPU-vs-CPU batch runner (`--pitches N
  --seed S`), reports outcome-type counts and fair-ball exit-velocity/
  launch-angle/carry-distance distributions.
- 26 tests total, including milestone 4's full validation suite
  (collision efficiency vs. the spec's own worked formula, carry distance
  vs. vacuum range, timestep convergence, an energy audit through
  contact, monotonicity of exit speed in swing speed -- alongside the
  pre-existing determinism/snapshot-roundtrip/zero-allocation tests) and
  a CPU-vs-CPU sanity test (no NaNs, no stuck states, physically bounded
  statistics).

Two real, non-obvious bugs were found and fixed during this work (see git
history / code comments for the full explanation): a CCD bug where a
fast-moving ball could tunnel deep into the bat before contact resolution
ever started (fixed by switching from a segment-segment "closest approach"
parameter to sampling+bisecting the actual surface gap along the swept
path), and a Hunt-Crossley energy-conservation bug where penetration depth
was re-derived from geometry every substep instead of being integrated as
a proper relative coordinate (verified by confirming energy still
increased even with zero damping, which should be physically impossible).
A third issue -- not a code bug, but a calibration mismatch -- surfaced a
real constraint: `PlayerInput`'s angular-velocity quantization is
calibrated for a real Joy-Con's documented gyro range, which a literal
0.65 m throwing-arm lever length can't reach real pitching speeds within
(see `sim/arm_properties.h`'s comment on `virtual_arm_length_m`).

Rendering and all input-hardware behavior remain **verified only on
Linux, only with synthetic/dev inputs** -- no real Joy-Con has been
available at any point in this project so far.

## Milestone 7 (local two-player on one screen)

New `src/versus/` library: join flow, role swap, per-player calibration
persistence, private controller feedback, and the shared-HUD privacy
contract, kept entirely out of `sim` (which still has no notion of
player slots -- it only ever sees role-keyed `PlayerInput`, same as
every earlier milestone) and out of `render` (the camera director is the
one exception that reads `SimState`, in the direction the architecture
spec allows).

- `sim/state.h` gains `ReadyState` (`pitcher_ready`/`batter_ready`
  latches + a wait-window start tick) and `step.h` gains
  `StepEvents::ready_for_next_pitch`, an edge-triggered signal fired once
  both roles' `kReady` bit have been seen or `SimConfig::
  ready_timeout_ticks` elapses. This is the one piece of milestone 7
  state that lives inside `SimState` itself, per spec ("a fixed,
  deterministic timeout applies so nobody can stall forever. This state
  lives in SimState") -- everything else below is input-layer/driver
  state, following the same split milestone 6 established for at-bat
  orchestration.
- `versus::RoleAssignment` / `should_swap_role`: which `PlayerSlot` holds
  which role, and a pure scheduling function (manual / every N at-bats /
  every half-inning) for when to swap. Deliberately outside `SimState` --
  the sim only ever sees `SimInputs::pitcher`/`::batter`, so slot-to-role
  mapping is decided identically by whoever is driving the sim, with no
  extra sim-level machinery needed.
- `versus::CalibrationStore`: gyro bias, virtual arm length, filter gain,
  and last recenter offset, one flat file per `PlayerSlot` (never per
  role or per device) under the platform pref path, same magic-header/
  fixed-buffer convention as `input::PlayerInputRecorder`.
- `versus::JoinFlow` + `step_or_pause`: an explicit
  unclaimed/active/disconnected state machine per slot, fully testable
  without SDL (a "simulated controller disconnect" is a direct
  `mark_disconnected()` call). `step_or_pause` calls `sim::step()`
  normally, or returns the previous state completely unchanged (tick
  does not advance, no input is synthesized) while any bound slot is
  disconnected. `versus::observe_hub` is the thin, untested-without-
  hardware glue that watches a real `SdlInputHub` and drives those
  transitions.
- `versus::PrivateFeedbackEvent` / `play_private_feedback`: a rumble
  pattern per grip preset plus a ready/recenter confirmation, routed to
  one player's controller only. `JoyconSource` gained `rumble()`/
  `set_led()` wrapping SDL3's `SDL_RumbleGamepad`/`SDL_SetGamepadLED` --
  SDL3 has no capability-query API for either (unlike SDL2's
  `SDL_GameControllerHasRumble`), so support is only knowable from the
  call's own return value. **Unverified on real hardware**, same as
  every other Joy-Con-dependent path in this project.
- `versus::PublicHud`: the shared-screen data contract (count, score,
  outs, last pitch result, speed after release only) -- enforced by
  having no field capable of carrying grip/pitch-type/aim data at all,
  not by a runtime filter. `versus::kSafetyReminder` is the wrist-strap/
  clear-space/keep-distance text shown at launch.
- `render::compute_camera`: a pure function of `(PitchPhase, SimState,
  CameraDirectorConfig)` -- behind-the-mound framing pre-release,
  center-field-high in flight, a follow cam after contact. Takes no
  grip/pitch-type/aim parameter, so it has no path to leak that
  information even by accident.
- `app/game` now drives both roles interactively: mouse (batter, as
  before) and keyboard/SPACE (pitcher's arm sweep + throw release, the
  second dev input path the input-layer README section always promised
  but this app never actually wired up). Both dev slots auto-join at
  startup (there's no join-flow UI yet), `step_or_pause`/`observe_hub`
  and `compute_camera` are wired into the frame loop, and the safety
  reminder prints once at launch. The shared HUD is still the milestone-3
  debug overlay, not a real `versus::PublicHud` -- there's no count/
  score/outs game loop in the interactive app yet (only in
  `tools/cpu_vs_cpu`/the at-bat tests), so there is nothing real yet for
  a play-mode HUD to display.
- 8 new tests: `sim_ready_up`, `camera_director`, `versus_role_swap`
  (including the spec's "role swap mid-session keeps SimState valid and
  hash-stable"), `versus_calibration_persistence` ("survives a role swap
  and a device swap"), `versus_disconnect_pause` ("pauses on a tick
  boundary and resumes cleanly"), `versus_hud_privacy` ("renders no
  pitch-type or aim information before release"), `versus_memory_budget`
  ("memory with two humans is within the per-player input budget of
  memory with zero" -- literally zero allocations either way, since
  every per-player structure here is fixed-size), and
  `two_human_replay_determinism` ("two recorded human input streams
  replayed together produce identical hashes on repeat runs"). 34 tests
  total.

Not yet built: the actual join-flow UI (press-a-button-to-join, a
reconnect prompt), a role-swap trigger wired into any real game loop, the
"tells" windup-animation flag (there's no per-grip animation content yet
for it to switch -- see `versus/config.h`'s comment), and a play-mode
`PublicHud` actually driving on-screen count/score/outs. These need
either a real at-bat game loop in `app/game` (beyond this milestone's
scope, which was the library + its validation tests) or real hardware to
design the join UI against.

## Building

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

SDL3 is fetched and built from source via `FetchContent` (pinned to
`release-3.4.16`), so the first configure will take a while; it does not
require a system SDL3 package. On Linux, `libudev-dev`, `libdbus-1-dev`,
`libx11-dev`, `libwayland-dev`, and `libxkbcommon-dev` should be installed
so SDL3 builds with its normal set of backends (all present on this dev
machine already). `libevdev-dev` is optional and only enables the extra
`--backend evdev` path in `imu_probe`.

## IMU probe

```sh
./build/tools/imu_probe/imu_probe [--seconds 15] [--backend evdev]
```

Reports, per connected gamepad: measured sample rate (from both the
device's own sensor timestamp and host arrival time), batching (samples
delivered per wakeup), an apparent quantization-step estimate, whether
button and IMU-sample timestamps share a domain, and the offset/drift
between the device's sensor clock and the host monotonic clock. Run it once
with one controller connected and again with two, and diff the printed
numbers to see whether a second device degrades things.

**Not yet run against real hardware.** This dev environment has no
Joy-Con/Pro Controller paired, so the tool has only been verified to build
and to exit cleanly (reporting "no gamepads detected") with zero devices
connected. Every rate/jitter/latency number the tool would print is
unverified until it's actually run against a real controller -- that's the
next concrete step, on a machine with a Joy-Con.

The `--backend evdev` path is Linux-only, optional, and scans
`/dev/input/event*` via `libevdev` for devices whose kernel-reported name
contains "Joy-Con" or "Pro Controller" (the in-kernel `hid-nintendo`
driver's naming). It requires read access to those device nodes -- on most
distros that means being in the `input` group.

## Heap profiling (for the "zero allocations" checks in later milestones)

- Linux: `heaptrack ./build/...` or `valgrind --tool=massif ./build/...`
- macOS: Instruments' Allocations template
- Windows: Visual Studio's built-in heap profiler, or Windows Performance
  Recorder/Analyzer

The allocation counter in `src/alloc` is the cross-platform check that runs
in CI; these tools are for deeper investigation when it fails.
