# gf_audio_core

Shared native C++ audio engine for **GrooveForge Sessions** (and reusable by
GrooveForge proper). The performance-critical audio path lives here; the Flutter
apps drive it through a thin C FFI surface.

> **License:** MIT (public / open-core), © 2026 Yann Poirier — same as GrooveForge.
> Permissive, so the closed-source Sessions app and GrooveForge can both link it
> freely. See [LICENSE](LICENSE).

## Why a standalone library

- The audio thread's correctness and latency are the whole product — keep them in
  one well-tested place, independent of any UI framework.
- **Pure core, swappable backend:** `gf_audio_core` (the DSP/timeline/mixer logic)
  has **zero external dependencies**, so it builds and unit-tests on a dev box with
  no audio hardware. Device I/O lives behind `IAudioBackend`; the default
  implementation uses [miniaudio](https://github.com/mackron/miniaudio) (covers
  Linux/ALSA today, Android/AAudio + iOS/CoreAudio later). Swap in raw Oboe /
  AVAudioEngine later without touching the engine.

## Layout

```
include/gf_audio_core/
  gf_audio_core.h        C FFI surface (the only thing Dart links)
  engine.h               realtime mixer/clock (the audio callback)
  transport.h            tempo / time-signature / bar-grid math
  metronome.h            grid-aligned click generator
  track.h                grid-aligned recorded take
  audio_backend.h        device I/O abstraction + miniaudio factory
  latency_calibrator.h   round-trip latency measurement (Phase 0 target)
  dsp/ring_buffer.h       lock-free SPSC ring buffer (RT-safe handoff)
src/                     implementations (+ src/backends/miniaudio_backend.cpp)
tools/gf_audio_demo.cpp  CLI: plays the metronome (toolchain + audio smoke test)
tests/                   dependency-free unit tests (transport math, ring buffer)
```

## Build & test (Linux dev)

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure   # logic tests (no audio hardware)
./build/gf_audio_demo 120 5                   # 5 s of metronome at 120 BPM
./build/gf_latency_probe                      # measure round-trip latency (speaker near mic)
```

The core library + tests need no network. Fetching miniaudio (for the backend,
demo, and FFI lib) happens at configure time; disable with
`-DGF_AUDIO_BUILD_BACKEND=OFF -DGF_AUDIO_BUILD_DEMO=OFF -DGF_AUDIO_BUILD_FFI=OFF`
to build the pure core offline.

## Realtime-safety rules (non-negotiable)

`Engine::process()` and everything it calls run on the OS audio thread and must
**never** allocate, lock, block, or do I/O. Control-thread → audio-thread
communication goes through `std::atomic` (tempo, transport, mute) or the lock-free
`SpscRingBuffer` (captured input). Anything that must allocate (loading a take,
re-rendering a time-stretch) happens off-thread and is handed over via those
primitives.

## Status — Phase 0 spike

The spike proves **latency-compensated overdubbing** on a single device before any
UI/sync work. Calibration leads with **OS-reported output latency** (per route) plus a
small input constant — it works with headphones and needs no user action; acoustic
loopback (`gf_latency_probe`) is a speaker-only verification/dev fallback.

Done:
- [x] Transport / bar-grid math (tested)
- [x] Lock-free SPSC ring buffer (tested)
- [x] Metronome (grid-aligned clicks) + working audio output via the demo
- [x] Track playback aligned to the grid
- [x] miniaudio duplex backend + C FFI surface
- [x] `LatencyCalibrator::estimate()` — normalized cross-correlation + parabolic
      sub-sample interpolation (tested: recovers a known delay to < 1 frame)
- [x] Engine record path: capture ring + record-start-head latch (`recordStartHead()`)
- [x] Overdub alignment (`recorder.h` `alignCapturedTake`): shifts a captured take
      earlier by the calibrated latency onto the grid (tested: impulses land on-grid)
- [x] Backend latency reporting: `IAudioBackend::outputLatencyFrames()` /
      `inputLatencyFrames()` — the PRIMARY calibration source (OS-reported in real
      Oboe/CoreAudio backends; buffer-based estimate in miniaudio)
- [x] `gf_latency_probe` — acoustic round-trip measurement (speaker-only
      verification/dev tool; demoted below OS-reported latency)

Next:
- [ ] Drive a full record -> align -> playback loop in the engine/demo: compensate by
      `outputLatencyFrames + inputLatencyFrames`, feed the aligned take into a `Track`,
      and hear it lock to the click
- [ ] Count-in (0/1/2 bars) before record+playback
- [ ] Real Oboe (Android) / AVAudioEngine (iOS) backends with OS-reported latency
- [ ] Manual ± ms nudge as the Bluetooth fallback
- [ ] Validate on real hardware: wired first, then Bluetooth
- [ ] Sub-sample (fractional-delay) shift for the take, not just whole frames
- [ ] Time-stretch integration (Signalsmith/SoundTouch) — pre-render on tempo change
```
