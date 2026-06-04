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
ctest --test-dir build --output-on-failure   # transport + ring buffer logic tests
./build/gf_audio_demo 120 5                   # 5 s of metronome at 120 BPM
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
UI/sync work.

Done (scaffold):
- [x] Transport / bar-grid math (tested)
- [x] Lock-free SPSC ring buffer (tested)
- [x] Metronome (grid-aligned clicks) + working audio output via the demo
- [x] Track playback aligned to the grid
- [x] miniaudio duplex backend + C FFI surface

Next (the actual spike):
- [ ] `LatencyCalibrator::estimate()` — cross-correlation round-trip measurement
- [ ] Record path: drain the capture ring into a `Track`, offset by the calibrated
      latency so the overdub lands on the grid
- [ ] Count-in (0/1/2 bars) before record+playback
- [ ] Validate timing on wired **and** Bluetooth output
- [ ] Time-stretch integration (Signalsmith/SoundTouch) — pre-render on tempo change
```
