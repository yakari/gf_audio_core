// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#pragma once
#include <functional>
#include <memory>

namespace gf {

// Abstraction over the platform audio I/O device. The engine implements the
// render callback; a backend (miniaudio today, raw Oboe/CoreAudio later) drives
// it from the OS audio thread.
class IAudioBackend {
 public:
  struct Config {
    double sample_rate = 48000.0;
    int buffer_frames = 256;   // requested period size; backend may adjust
    int input_channels = 1;    // 0 = playback only
    int output_channels = 2;
  };

  // Called from the realtime audio thread. `in` may be null when there is no
  // input. Both buffers are interleaved float; `num_frames` frames each.
  // MUST be RT-safe.
  using RenderFn = std::function<void(const float* in, float* out, int num_frames)>;

  virtual ~IAudioBackend() = default;

  virtual bool start(const Config& config, RenderFn render) = 0;
  virtual void stop() = 0;
  virtual bool isRunning() const = 0;
  virtual double actualSampleRate() const = 0;
  virtual int actualBufferFrames() const = 0;

  // Reported latency of the current route, in frames. The record path shifts a
  // captured take earlier by (outputLatencyFrames + inputLatencyFrames) so it
  // lands on the grid (see recorder.h) — this is the PRIMARY calibration source,
  // and unlike acoustic loopback it works with headphones.
  //
  // Output latency is the dominant, route-dependent term (small wired, large over
  // Bluetooth); input latency is small and stable for internal/USB mics. Real
  // backends return OS-reported values (Oboe calculateLatencyMillis() / iOS
  // AVAudioSession.{output,input}Latency); the miniaudio backend can only
  // estimate from internal buffer/period sizes, which is fine for desktop dev.
  virtual double outputLatencyFrames() const = 0;
  virtual double inputLatencyFrames() const = 0;

  // Human-readable description of the last start() failure, or "" if none.
  // Owned by the backend; valid until the next start().
  virtual const char* lastError() const = 0;
};

// Factory implemented by the miniaudio backend translation unit.
std::unique_ptr<IAudioBackend> createMiniaudioBackend();

}  // namespace gf
