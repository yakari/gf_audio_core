// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

// Measures real round-trip (output -> air -> input) latency on this device by
// playing a click and cross-correlating what the mic records against it.
//
//   ./gf_latency_probe
//
// This is an ACOUSTIC loopback: hold the device so the SPEAKER is near the mic
// (do NOT use headphones — the mic can't hear them, so there's nothing to
// detect). It reports the combined output+input latency the record path must
// compensate for. Run it a few times; a stable reading is a good calibration.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "gf_audio_core/audio_backend.h"
#include "gf_audio_core/latency_calibrator.h"

using namespace gf;

namespace {

// Shared state for the probe's audio callback. The capture buffer is
// pre-allocated, so the callback never allocates (stays RT-safe).
struct ProbeState {
  std::vector<float> stimulus;   // the click we emit, once
  std::vector<float> capture;    // mono recording, pre-sized
  size_t capture_pos = 0;        // audio-thread write cursor
  int64_t frame = 0;             // running frame counter
  int64_t stimulus_start = 0;    // frame at which to begin emitting the click
  int input_channels = 1;
  int output_channels = 1;
};

// Realtime callback: record the mic, emit the stimulus once at stimulus_start.
void render(ProbeState* p, const float* in, float* out, int num_frames) {
  for (int i = 0; i < num_frames; ++i) {
    // Capture channel 0 of the input into the mono record buffer.
    const float sample = in != nullptr ? in[static_cast<size_t>(i) * p->input_channels] : 0.0f;
    if (p->capture_pos < p->capture.size()) p->capture[p->capture_pos++] = sample;

    // Emit the stimulus on all output channels when we reach its window.
    float o = 0.0f;
    const int64_t rel = p->frame - p->stimulus_start;
    if (rel >= 0 && static_cast<size_t>(rel) < p->stimulus.size())
      o = p->stimulus[static_cast<size_t>(rel)];
    for (int c = 0; c < p->output_channels; ++c) out[static_cast<size_t>(i) * p->output_channels + c] = o;

    ++p->frame;
  }
}

}  // namespace

int main() {
  const double sr = 48000.0;
  const int buffer_frames = 256;
  const int input_channels = 1;
  const int output_channels = 1;

  ProbeState probe;
  probe.input_channels = input_channels;
  probe.output_channels = output_channels;
  probe.stimulus = LatencyCalibrator::makeStimulus(sr);
  probe.capture.assign(static_cast<size_t>(sr * 2.0), 0.0f);  // 2 s of headroom
  probe.stimulus_start = static_cast<int64_t>(sr * 0.25);      // 250 ms warm-up

  std::printf("Latency probe — acoustic loopback (speaker near mic, NOT headphones)\n");

  auto backend = createMiniaudioBackend();
  IAudioBackend::Config cfg;
  cfg.sample_rate = sr;
  cfg.buffer_frames = buffer_frames;
  cfg.input_channels = input_channels;
  cfg.output_channels = output_channels;

  if (!backend->start(cfg, [&](const float* in, float* out, int n) { render(&probe, in, out, n); })) {
    std::fprintf(stderr, "Failed to start audio backend (no device / permission?)\n");
    return 1;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(1500));
  backend->stop();

  const LatencyCalibrator::Result r = LatencyCalibrator::estimate(probe.stimulus, probe.capture, sr);
  if (!r.valid) {
    std::printf("No confident detection (confidence %.2f). Is the mic hearing the speaker?\n",
                r.confidence);
    return 2;
  }
  const double ms = r.round_trip_frames * 1000.0 / sr;
  std::printf("Round-trip latency: %.1f frames = %.2f ms  (confidence %.2f)\n", r.round_trip_frames,
              ms, r.confidence);
  return 0;
}
