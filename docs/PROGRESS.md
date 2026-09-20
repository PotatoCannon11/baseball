# Running progress log

This is a living handoff document for any agent (on any platform: Linux,
macOS, Windows) picking up work on this project. Read `docs/ORIGINAL_PROMPT.md`
first for the full spec -- this file is a status snapshot plus an append-only
log, not a replacement for it. `README.md` has the detailed per-milestone
technical writeup; this file is shorter and focused on "what's the state right
now and what should the next agent do."

**Convention: append, don't rewrite.** When you finish a session of work, add
a new dated entry at the bottom of the Log section below. Don't delete or
rewrite other agents' entries. Update the "Current snapshot" section at the
top only when it goes stale (new milestone, new platform verified, etc.) --
and note in your log entry that you did so.

---

## Current snapshot

**As of:** 2026-09-20 (checked by a Claude Sonnet 5 agent on Linux)
**Branch:** `main`, up to date with `origin/main` (`https://github.com/PotatoCannon11/baseball`)
**Latest commit:** `c476163` -- "Fix sim permanently paused from tick 0 by observe_hub on dev-only slots"
**CI:** green on `main` (GitHub Actions, matrix: ubuntu gcc/clang, macos clang, windows msvc)
**Local build/test (Linux, this checkout):** 34/34 tests pass (`ctest --test-dir build --output-on-failure`)
**Open GitHub issues/PRs:** none

**Milestone status:** Milestone 7 (local two-player on one screen) is built
and its library-level tests pass. See README.md's "Milestone 7" section for
what's built and its "Not yet built" paragraph for what's explicitly deferred
(join-flow UI, role-swap trigger wired into a real game loop, tells-flag
content, a real play-mode `PublicHud`). Milestone 8 (online-readiness harness,
loopback transport, no real sockets) has not been started.

**The big open risk:** almost everything Joy-Con/hardware-related and
everything about the interactive `app/game` executable is **only verified on
Linux, with synthetic or dev (mouse/keyboard) input** -- no physical Joy-Con
has ever been available in any dev environment used on this project so far,
and no macOS or Windows machine has run the interactive app. If you are
working from a macOS or Windows machine, or have a real Joy-Con/Pro
Controller, that is the highest-value thing you can do: run the app
interactively and/or `tools/imu_probe` against real hardware and report back
here, per `docs/ORIGINAL_PROMPT.md`'s instruction to never claim something
works on a platform you didn't run it on.

### Recently fixed: the "no input works" bug

Commits `763eb2f`, `d754409`, `c476163` (2026-09-19/20) chase a single
reported symptom: "no keyboard/mouse input has any effect in `app/game`."
The root cause (`c476163`) turned out to be platform-independent, not
macOS-specific as first suspected: `app/game/main.cpp` called
`versus::observe_hub()` every frame against dev (mouse/keyboard) player
slots that were never registered in `SdlInputHub`'s real-gamepad binding
table, so `observe_hub()` immediately marked both slots "disconnected" and
`step_or_pause()` paused the sim from tick 0 forever. Input was always
reaching the input layer fine -- the sim underneath it just never advanced.
This is now fixed (stopped calling `observe_hub()` on dev-only slots until a
real join-flow UI exists to legitimately bind hardware to a slot).
`763eb2f`'s click-through/focus-raise fix for the *originally suspected*
macOS first-click issue is still in place and still worth having, but is
**itself unverified on macOS** -- nobody has run the app on a Mac since
either fix landed. If you're on macOS: run `app/game`, confirm input now
works, and update this file either way.

### Unverified items carried over from README (still true as of this snapshot)

- Real barrel-speed number from an actual hard swing (spec expects ~25-40 m/s).
- Left/right Joy-Con axis mapping (`device_profile.h`'s `axis_mapping_verified = false`).
- Gyro/accel clip range table (from public reverse-engineering references, not an official spec).
- `bat_model.h`'s `kDefaultArmLengthMeters = 0.75` placeholder -- needs a real calibration-swing design.
- `JoyconSource::rumble()`/`set_led()` (SDL3 has no capability-query API for either).
- `tools/imu_probe` has never been run against real hardware (only "0 gamepads detected" verified).
- Everything in `app/game`'s interactive loop on macOS and Windows (build/CI only, no interactive run).

---

## How to build and test (any platform)

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

SDL3 is fetched via `FetchContent` (pinned `release-3.4.16`) -- no system SDL3
needed. Linux needs `libudev-dev libdbus-1-dev libx11-dev libwayland-dev
libxkbcommon-dev` (`libevdev-dev` optional, enables `imu_probe --backend
evdev`). See `.github/workflows/ci.yml` for the exact per-OS dependency
install steps CI uses (Homebrew `ninja` on macOS, `choco install ninja` +
`ilammy/msvc-dev-cmd` on Windows).

CI runs `ctest --test-dir build --output-on-failure -E render_smoke`
(`render_smoke` needs a real/virtual display, so CI excludes it; it passes
locally on this Linux box under whatever X/Wayland session is active --
worth trying without the exclusion on a new platform to see if it needs the
same treatment there).

Tools worth running by hand on a new platform:
- `./build/tools/imu_probe/imu_probe [--seconds 15] [--backend evdev]` -- with
  a real controller connected, once with one device and once with two, per
  `docs/ORIGINAL_PROMPT.md`'s milestone-1 instructions.
- `./build/tools/barrel_speed/barrel_speed --backend mouse|keyboard` for
  dev-input testing without a controller, or without `--backend` for a real
  Joy-Con.
- `./build/app/game/game` -- the interactive executable. Mouse drags the
  batter, keyboard/SPACE drives the pitcher's arm sweep + release. Set
  `BASEBALL_DEBUG_INPUT=1` in the environment if input seems dead, to log
  every SDL event the hub receives (see the "no input" bug above).

---

## Suggested next steps (pick based on what your platform/hardware gives you)

1. **If you have a real Joy-Con/Pro Controller (any platform):** run
   `imu_probe` and `barrel_speed`, report real numbers against the items in
   "Unverified items" above. This unblocks more of milestone 2/7 than
   anything else on the list.
2. **If you're on macOS:** run `app/game` interactively, confirm the input
   fix actually works, verify Joy-Con L/R axis mapping if hardware is
   available, measure process memory (README/ORIGINAL_PROMPT ask for a
   macOS-measured memory target since driver overhead is unmeasured there).
3. **If you're on Windows:** same as macOS -- first-ever interactive run of
   `app/game` and CI is MSVC-only so far, no manual verification.
4. **If no hardware/other-platform access:** the next unstarted scope is
   either finishing milestone 7's deferred items (join-flow UI,
   role-swap-trigger wiring, a real play-mode `PublicHud`) or starting
   milestone 8 (loopback `Transport` interface simulating latency/jitter/
   loss/reordering, two in-process sessions, resimulation test). Read
   `docs/ORIGINAL_PROMPT.md`'s Milestones section (8) and README's "Not yet
   built" paragraph before starting either.

---

## Log

Append new entries below, newest at the bottom. Format: date, platform,
who/what, what changed, what's still open.

### 2026-09-20 -- Linux -- Claude Sonnet 5 (background session)

Audited current repo state against `docs/ORIGINAL_PROMPT.md` and
`README.md`: confirmed working tree clean, `main` up to date with origin,
CI green on latest commit, no open issues/PRs, and a local
`cmake --build` + `ctest` run reproduces 34/34 passing. No code changes.
Created this file so other agents (especially on macOS/Windows, or with
real Joy-Con hardware) have a single place to check current status and
log their own findings without needing to re-derive it from git history
and README diffing. See "Current snapshot" above for details as of this
entry.
