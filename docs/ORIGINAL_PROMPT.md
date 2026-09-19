# Original project prompt

Saved verbatim on 2026-09-18 as the reference spec for this project. When in
doubt about scope, priorities, or a design rule, check here before guessing.

---

# Physics-first motion-controlled baseball sandbox
(C++, Linux + macOS + Windows, no engine, CPU opponents, local versus on one
screen, online-ready design)

You are building this from scratch with Claude Code. Work milestone by
milestone. Do not start a milestone until the previous one builds, runs, and
passes its checks. After each milestone, summarize what works, what you
measured, and what you are unsure about. Never invent hardware or driver
behavior: where I say "measure", write a small tool that measures it and
report the result. You can only run code on the machine you are on. Never
claim something works on a platform you did not run it on; label it
"untested on <platform>" and provide CI configuration (GitHub Actions matrix
for Linux, macOS, Windows) so I can verify.

## Goal
A 3D game where Joy-Con motion actually determines the outcome. Swing speed,
bat angle, and contact point come from the real controller and the bat-ball
collision is fully physics-based. Throwing works the same way: arm motion sets
release speed, and the way you throw and grip sets spin. Ball size, mass,
surface, and bat properties are editable at runtime. Either role (pitcher,
batter) can be human, CPU, or (later) a remote player. Two humans can play
against each other locally on ONE shared screen with NO split screen, like
Mario Super Sluggers: one pitches, the other bats, both watch the same view.
The simulation must be structured so online play can be added properly later
without a rewrite.

## Platforms and build
- C++20, CMake + Ninja. Compilers: GCC, Clang, MSVC (or clang-cl). No
  compiler-specific extensions in the sim library. No POSIX-only or
  Win32-only code outside src/platform/, which exposes small interfaces:
  process memory query (Linux /proc, macOS task_info, Windows private bytes,
  not working set), high-res clock, file-change polling, base and pref paths
  (SDL_GetBasePath, SDL_GetPrefPath), and threading primitives if needed.
- Dependencies allowed: SDL3 (window, GL context, timing, gamepad and sensor
  input, HIDAPI Joy-Con driver), a GL loader (glad), stb_image. Optional
  Linux-only backend: libevdev. Nothing else without asking. No networking
  library now.
- Rendering baseline: OpenGL 4.1 core, because macOS caps at 4.1. No DSA, no
  glTexStorage, no compute, no SSBO, no 4.2+ features. Allocate texture
  storage once with glTexImage2D and update with glTexSubImage2D on reload.
  Use the macOS forward-compatible core-profile flag. Anisotropic filtering
  only if the extension is present. Keep all GL behind a thin renderer
  interface so SDL_GPU or Metal can replace it later.
- Hot-reload uses portable mtime polling (about every 500 ms) on a fixed file
  list with fixed path buffers. Native watchers (inotify, FSEvents,
  ReadDirectoryChangesW) are optional and only inside src/platform/.
- SI units everywhere. Physics state in double, render data in float.
- No exceptions or RTTI in hot paths. No virtual calls, std::function, or
  heap allocation inside the frame or simulation loop.

## Memory efficiency (top priority, treat as a feature)
- Two budgets: (a) the app's own allocations must fit a compile-time budget
  table (target 32 MB total including snapshot ring and all arenas); (b)
  total process memory: target under 100 MB on Linux and Windows. Measure
  macOS and propose a target from the measurement, because driver overhead
  differs. Report both numbers every milestone using the platform memory
  query, in a debug overlay and in a test.
- Zero allocations after startup in the sim and frame loops. One startup
  arena sized from the budget table, plus fixed-capacity containers (ring
  buffers, small vectors) everywhere else. Add an allocation counter
  (override operator new/delete and wrap any malloc we call ourselves) and a
  test that fails if anything is allocated during a 60-second simulated run.
  Library and driver allocations are not counted, but show up in process
  memory.
- IMU ring buffers: one per device, fixed power-of-two capacity, plain
  structs, about 1 s of history.
- Local versus must not add render memory: one window, one viewport, one
  camera, one shadow map, one set of meshes and textures regardless of how
  many humans play. A test compares memory with zero, one, and two human
  players and fails if the difference exceeds a small per-player input budget.
- Textures: decode with stb_image, upload, free the CPU copy immediately.
  Cap ball textures at 1024x1024, sRGB, generated mipmaps. Reuse the same GL
  texture object on reload. Asset reload uses a preallocated scratch buffer
  outside the sim and frame hot paths.
- Meshes: generate icosphere, bat lathe mesh, and field once into shared
  vertex/index buffers with packed formats; free scratch after upload.
- One depth-only shadow map (start 2048^2), reused each frame.
- static_assert on sizeof for hot structs. Logging and the debug overlay use
  fixed char buffers, no per-frame std::string building.
- Provide heap profiling instructions per OS (heaptrack or massif on Linux,
  Instruments on macOS, Visual Studio or Windows tooling on Windows). The
  allocation counter is the cross-platform check.

## Architecture: simulation is separate from everything else
Build the simulation as its own static library ("sim") that depends only on
the C++ standard library and a small math wrapper. It must not include SDL,
GL, platform headers, read clocks, touch the filesystem, or know about input
devices. Enforce this through CMake target dependencies.

- Fixed tick rate (constant, e.g. 240 Hz; choose and justify from measured
  step cost). Ticks are counted as uint32. Adaptive substeps near contact are
  allowed but must be a deterministic function of state only. Rendering
  interpolates between the last two sim states.
- The sim is a pure step: next_state = step(state, inputs, config). All
  state lives in one POD, trivially copyable, fixed-size struct (SimState)
  with no pointers (use indices), containing tick number, ball, bat, arm,
  at-bat state, role assignments, RNG state, and CPU brain state.
- Inputs: one fixed-size, versioned POD PlayerInput per role per tick:
  quantized fused bat/arm orientation and angular velocity, buttons, stick,
  clip flags, and a sequence number. Raw IMU data and sensor fusion live in the
  input layer, outside the sim. The input layer interpolates its samples to
  sim tick times.
- Snapshots: save/load are a memcpy of SimState. A fixed-capacity ring holds
  the last N snapshots (N chosen for at least 250 ms of history). Provide a
  state hash function for desync detection.
- Config vs. state: ball, bat, and grip property structs are immutable
  during a tick, carry a version and hash, and are agreed at session start.
  Live edits from the property editor become timestamped sim commands applied
  at a specified tick, never direct mutation.
- Output events (release, contact, bounce, call, rumble request, LED request)
  go into a fixed-capacity per-tick buffer consumed by rendering, audio, and
  haptics. Each event carries a target player/role and a visibility flag so
  private feedback reaches only one controller. Presentation never writes
  into the sim.
- Wire format: explicit, versioned, endian-defined serialization. Never send
  raw struct bytes over a wire. (Local memcpy snapshots are fine.)
- Deterministic pitch flight: a release message (release tick, ball state)
  must be enough for any peer to reproduce the flight. On receipt, a peer
  fast-forwards the flight to the current tick.

## Determinism rules
- Same binary, same inputs, same outputs, bit for bit (test required).
- No -ffast-math. Set -ffp-contract=off explicitly (Clang fuses
  multiply-adds by default on Apple Silicon; use the MSVC equivalent). No
  -march=native on the sim library. No data-dependent SIMD paths. No
  unordered-container iteration, pointer-address ordering, threads, or clock
  reads inside the sim.
- RNG (PCG or xoshiro) is explicit, seeded, and stored in SimState.
- Route all transcendental functions (sin, cos, exp, pow, atan2, etc.)
  through a small math wrapper that calls libm now but can be replaced with
  deterministic implementations later. Do not promise cross-platform
  bit-exactness. Write a tool that prints a state hash after a scripted run
  so I can compare across platforms, and report whether hashes match on the
  machines available.
- Design target: authoritative state plus snapshots and hash checks, not
  blind lockstep. Keep lockstep/rollback possible.

## Roles and controllers
Two roles, Pitcher and Batter. Each role is bound to a controller source:
Local(device), Cpu(profile), Scripted(recorded input file), and later
Remote. The sim only ever sees PlayerInput. It cannot tell the sources
apart. Any combination is legal: human vs CPU, human vs human, CPU vs CPU
(headless), and later local vs remote.
- Development inputs that need no Joy-Con: mouse-drag swing and throw, a
  keyboard mapping for a second dev player, and scripted input playback. These
  exercise the same input path and let macOS/Windows/CI work without
  hardware.
- Input recording: record and replay PlayerInput streams from a file (fixed
  buffers, no per-sample allocation). Recorded real swings become regression
  tests.

## Local versus on one screen (no split screen)
Two humans, one window, one camera, one shared view. The sim already treats
roles as input sources, so this is an input-binding and presentation feature.
It must not fork the sim or the renderer.

Controller binding
- Each human holds ONE controller: a single Joy-Con held sideways works for
  swinging and throwing (Joy-Con L and R have mirrored axes; handle this in
  the device profile table). A Switch Pro controller or a paired Joy-Con
  set also counts as one controller. Verify how SDL3 exposes single vs.
  combined Joy-Cons on each OS and report it. Do not assume.
- "Press a button to join" flow: each controller is claimed by a player slot
  (P1, P2). Slots are bound to roles (Pitcher/Batter). Keep the slot-to-device
  binding in a small fixed table in the input layer, never in the sim.
- Hot-plug and disconnect: if a bound controller drops during play, pause
  deterministically at the next tick boundary, show a reconnect prompt, and
  resume on reconnect or let the player switch to a CPU. Never crash and never
  let a missing device inject zeros into a live pitch.
- Per-player calibration: the gyro bias, virtual arm/bat length, gain curve,
  and recenter are stored per player slot (in a fixed struct saved in the
  pref path), not per device, so swapping controllers doesn't lose settings.
  Each player recenters independently.
- Two devices at once: each has its own IMU ring buffer, its own sensor
  clock offset and drift estimate against the host clock, and its own
  timestamp lookup for button events. Do not share one timebase estimate
  between devices.

Roles and turns
- Role swap: the roles swap between human players on a configurable schedule
  (each N at-bats, or on a "half-inning" boundary, or manual). The swap is a
  sim command that changes role bindings at a specified tick, and player
  calibration follows the player, not the role. Provide an option for
  "same roles all game" for sandbox testing.
- Ready-up: a pitch cycle starts when both bound humans signal ready (or the
  CPU auto-readies). A fixed, deterministic timeout applies so nobody can
  stall forever. This state lives in SimState.
- Simultaneous action is normal: the batter can be mid-swing while the
  pitcher releases. Both inputs are sampled at the same tick boundary through
  the same path, with no priority for either player.

One shared view, fairness, and hidden information
- One window, one viewport, one camera. Absolutely no split screen or
  picture-in-picture. Camera framing is a pure function of sim state and
  presentation config (behind-the-mound or center-field-high view during the
  pitch and swing, cutting to a follow cam after contact, then back). It never
  feeds back into the sim and never depends on which players are human.
- The shared screen must not leak private decisions. The pitcher's selected
  pitch type, grip, and aim target must NOT be drawn on screen before release,
  and the batter's intended swing timing or aim must not be drawn either.
  Private feedback goes through the controller instead: a distinct rumble
  pattern per selected grip, and a player-LED or short haptic confirmation,
  emitted as private output events (player-targeted, visibility flag) and
  played only if the platform and driver support it. Report what SDL3
  supports on each OS.
- Optional "tells" (windup animation differences per grip) are a config flag
  for difficulty and default to off, so a human batter isn't given the pitch
  type for free. What the ball does after release (visible spin from the
  spinning texture, flight path) is public and always shown.
- Shared HUD only shows public information: count, score, outs, last pitch
  result, speed after release. Debug overlay is dev-only and hidden in play
  mode.
- Safety: on first launch and in the join flow, show a short reminder to use
  wrist straps, clear the space around each player, and keep distance between
  players, since two people are swinging in one room.

## CPU pitcher and batter
CPU players obey the same physics and the same interfaces. They never place
the ball, force an outcome, or read hidden state a human could not know. Since
humans can now be on both sides, a CPU pitcher's private choices are as hidden
from a human batter as a human pitcher's would be.
- Pitcher: a repertoire (grip presets), a target chosen in the strike-zone
  grid by count and strategy, and an aim solver that finds release velocity
  and spin through a shooting method over the real flight model with a fixed
  iteration count and no allocation. Execution error (speed, direction, spin)
  scales with difficulty and fatigue. It emits the same release data a human
  throw produces (through the same throw code), with a windup of fixed
  duration so a batter can react.
- Batter: perceives the ball only after a reaction delay and with noise;
  estimates the trajectory with the flight model using imperfect knowledge of
  spin and pitch type (more uncertainty for breaking pitches); decides
  swing/take from an estimated zone position and count; plans a swing (start
  tick, plane, contact point near the bat's sweet spot, bell-shaped angular
  velocity profile) and executes it as a stream of PlayerInput frames. Swing
  effort trades speed against timing and aim error.
- Difficulty is a config struct: reaction time, perception noise, timing
  error, aim error, effort behavior, pitch control error, fatigue. All
  randomness comes from the sim RNG. CPU brain state lives in SimState.
- The Milestone 4 pitching machine is the simplest CPU pitcher.
- Headless CPU-vs-CPU mode (no window, no GL) runs thousands of at-bats for
  validation and statistics.

## At-bat loop (sandbox scope)
Pitch, swing or take, then classify: called ball, called strike, swinging
strike, foul (simple 90-degree fair-territory wedge from home plate), or fair
ball with distance, exit velocity, and launch angle. No fielders, baserunning,
or innings for now, other than the role-swap schedule above. Strike zone is a
config box over the plate.

## Input (Joy-Con), cross-platform
1. Primary path: SDL3 gamepad sensor API (gyro and accelerometer) with the
   HIDAPI driver. Optional Linux backend: evdev via libevdev/epoll. Define an
   ImuSource interface with a device-profile table (gyro range, nominal rate,
   axis mapping including left/right Joy-Con orientation, sensor units) so
   Joy-Con, Switch Pro, DualSense, and later Wiimote+MotionPlus can slot in.
   Do not hardcode the +-2000 deg/s clip threshold. Read it from the profile.
2. FIRST write a measurement tool, run per platform: actual sample rate,
   batching (samples per report), gyro range and resolution, whether button
   and IMU events share a timestamp (on the SDL/HID path they should come from
   the same report, so verify), event pump latency, and clock offset and drift
   between the device sensor timestamp and the host monotonic clock. Run it
   with ONE device and again with TWO devices connected at once, and report
   whether sample rate, latency, or dropped reports get worse with two. If the
   IMU is only about 60 Hz, timestamps are unusable, or two devices degrade
   badly, tell me before continuing and propose alternatives. Note any OS
   permission or pairing requirements you encounter (e.g., macOS Input
   Monitoring, Windows Bluetooth pairing).
3. Timestamps: correlate buttons and IMU through a ring buffer per device. A
   release event looks up swing state at its timestamp, not "now". If pumping
   events once per frame adds too much latency, pump more often between sim
   ticks (note that some platforms require the main thread).
4. Gyro bias: measure at rest at startup and subtract. Apply factory
   calibration if accessible, otherwise document why not.
5. Orientation: Madgwick or Mahony filter. Correct tilt from the
   accelerometer only when its magnitude is near 1 g. Yaw has no reference, so
   add a recenter button.
6. Saturation: detect clipping using the device profile, flag frames, and
   recover the true peak by fitting a smooth bell-shaped curve to the
   unclipped samples on both sides. Tunable gain and soft response curve. A
   calibration swing sets the virtual arm and bat length.
7. Bat model: a rigid rod pivoting about a fixed virtual pivot. Barrel
   velocity is omega x r. No double integration of acceleration.

## Collision (no raycast movement)
Nothing is moved by raycasts. The bat is kinematic, driven by PlayerInput. The
ball is integrated. Each substep, test the ball center's swept segment
against the bat as a chain of capsules/cones inflated by ball radius (plus
segment vs. plane for ground, walls, backstop, plate). No scene query system
and no spatial acceleration structure. The contact function must be pure and
callable from any peer (bat pose stream + ball state in, result out).

## Bat and ball physics
- Bat: lathe profile (radius vs. length) plus mass, center of mass, and
  moment of inertia. The render mesh and collision chain come from the same
  data.
- Contact: Hunt-Crossley force F = k x^n + lambda x^n xdot,
  integrated at microsecond substeps during contact, so restitution falls with
  impact speed. Tangential stick-slip friction with tracked tangential
  deformation. Effective mass at contact 1/(1/m + (r x n)^2 / I). Later polish:
  3-4 damped bat vibration modes.
- Flight: RK4 in double. Reynolds-dependent drag with a surface-roughness
  input. Magnus force from the transverse spin component only (omega x v).
  Spin decay from aerodynamic torque. Added mass and buoyancy. Air density
  from temperature, pressure, humidity, altitude. Wind with height dependence
  and gusts (deterministic, from the sim RNG). Seam force depending on ball
  orientation (knuckleball behavior).
- Ground: restitution plus tangential friction with sliding-to-rolling
  transition, rolling resistance, presets (grass, turf, dirt).
- The ball keeps an orientation quaternion integrated from angular velocity.

## Throwing
- The arm is a pivoting rod. Release speed = arm angular velocity x virtual
  arm length. Release is a shoulder-trigger button-up event, looked up at its
  timestamp. The throw code consumes a ReleaseState struct (hand velocity,
  grip, wrist and finger inputs, release tick) that a human input and a CPU
  planner both fill.
- Heavier or bigger balls need more effort: v = v_measured * sqrt((M_arm +
  m_ref) / (M_arm + m)), with M_arm exposed as a tunable. Rumble requests
  scale with ball mass at release (emitted as a private output event to the
  pitcher's controller, played only if supported).
- Spin: face-button grip preset selects a 3x3 matrix from controller angular
  velocity to ball spin, plus default axis, efficiency, and speed penalty.
  Wrist motion at release (forearm-axis component, wrist snap) blends into the
  spin axis. The stick offsets finger contact point. Spin = finger impulse x r
  / I with I = k m r^2, capped by maximum finger impulse.

## Editable properties
Plain structs loaded from a config file: Ball, Bat, Grip, CPU difficulty
profiles, per-player calibration. Visual texture is separate from physics
preset, with an optional linked "surface" preset. Live edits are sim commands
(see Architecture). Textures hot-reload from a folder. In two-human play, only
allow edits between at-bats, and both players see when properties changed.

## Rendering
OpenGL 4.1 core. Simple readable 3D: field, ball, bat, sky, directional
light, one shadow map (the ball's shadow is the key depth cue). Small
shaders. Single camera and single viewport for all play modes. Debug overlay
(dev-only): bat tip speed, ball speed, spin, exit velocity, app allocation
total, process memory, frame time, sim step time, tick, state hash.

## Design for a future autodiff tool (do NOT build it now)
Write flight and contact code as templates on a scalar alias, no hidden global
state, fixed-step integration, no data-dependent allocation. Keep hard
switches (hit/miss, stick/slip) isolated in small functions so they can be
smoothed later. Flag any place this conflicts with performance and ask me.

## Milestones
1. Platform and measurement harness: CMake for all three OSes, CI matrix,
   platform layer (memory query, clock, paths), allocation counter, IMU
   measurement tool (SDL3 path, plus evdev on Linux) tested with one and two
   devices. Report per platform.
2. Input layer: ImuSource, device profile (including L/R Joy-Con), per-device
   timestamped ring buffers, bias calibration, orientation filter, clip
   recovery, multi-device claim and binding table, mouse/keyboard and
   scripted input sources, PlayerInput recording and replay. Terminal output
   of bat barrel speed while I swing (hard swing roughly 25-40 m/s at the
   barrel).
3. Sim skeleton and bare 3D scene: SimState, tick loop, snapshot ring, state
   hash, math wrapper, determinism test, headless executable, then the
   renderer showing interpolated state (ball, shadow, lathe bat, camera,
   overlay).
4. Continuous collision, compliant contact, and flight with a pitching
   machine. Validation tests (below).
5. Throwing: arm model, release timing, grips, spin, mass and size scaling.
6. CPU pitcher and batter, at-bat loop, headless CPU-vs-CPU batches with
   sanity statistics (exit velocity distribution, launch angles, foul and
   strike rates, no NaNs, no stuck states).
7. Local versus on one screen: join flow, slot-to-role binding, role swap
   schedule, ready-up, disconnect/reconnect pause, per-player calibration
   persistence, single shared camera, private controller feedback with no
   pitch or aim information on the shared screen, tells config flag, safety
   reminder. Works with any mix of human, CPU, and dev inputs. Run the memory
   test for 0, 1, and 2 humans.
8. Online readiness harness (no sockets): a Transport interface with a
   loopback implementation that simulates latency, jitter, loss, and
   reordering; two in-process sessions; state-hash desync detection; a
   snapshot-based resimulation test proving the sim can re-run about 12 ticks
   within the frame budget; release-message fast-forward test. Report step cost
   and margin.
9. Property editor, config file, texture hot-reload (edits as sim commands).
10. Polish: bat vibration modes, wind, seam effects, ground surfaces.

## Validation tests (must pass before moving past milestone 4, unless noted)
- Collision efficiency: exit speed = q v_pitch + (1+q) v_bat, q = (e - r)/(1
  + r), r = m_ball / M_eff. For q about 0.2, a 90 mph pitch and 70 mph bat
  gives roughly 100 mph.
- Carry: a well-hit fly ball travels roughly a third to a half less than
  vacuum range.
- Convergence: halving the timestep barely changes results.
- Energy audit through contact.
- Determinism: same inputs give identical outputs and hashes, bit for bit.
- Monotonicity: a harder swing never produces a shorter hit at fixed
  contact geometry.
- Memory: zero counted allocations during a 60 s simulated run; report app
  and process memory.
- Snapshot round-trip: save, run N ticks, load, rerun, hashes equal.
- Milestone 7: two recorded human input streams replayed together produce
  identical hashes on repeat runs; a role swap mid-session keeps SimState
  valid and hash-stable; a simulated controller disconnect pauses on a tick
  boundary and resumes cleanly; the shared screen renders no pitch-type or
  aim information before release (test the draw list or overlay contents);
  per-player calibration survives a role swap and a device swap; memory with
  two humans is within the per-player input budget of memory with zero.

## Working style
Keep code simple and readable, commented where physics needs explaining (cite
the equation). Ask before adding dependencies or making major architectural
changes. Prefer small verified steps. If something is uncertain (hardware
behavior, driver quirks, a coefficient, a platform you cannot test), say so
and measure or cite instead of guessing.

---

## Follow-up instruction (given at the start of milestone 1 work)

"do milestone one, avoid needing remotes until absolutely needed. also save
the original prompt as an md to refer to for this project"

Interpretation: build milestone 1 in full, but keep the `Remote` controller
source and any networking concerns out of scope until a milestone explicitly
requires them (milestone 8 introduces the loopback `Transport` harness with
no real sockets; actual remote play is not in this milestone list at all).
This file is the artifact satisfying the second half of that instruction.
