// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

// Records ~6 s from the mic while playing the metronome, latency-aligns the take
// onto the grid, then plays it back locked to the click — so you can verify
// duplex capture + alignment on real hardware before any Flutter integration.
//
//   ./gf_record_demo [record_seconds] [playback_seconds]
//
// Use headphones to avoid the click bleeding into the recording.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "gf_audio_core/audio_backend.h"
#include "gf_audio_core/engine.h"

using namespace gf;

namespace {
bool startBackend(IAudioBackend& backend, Engine& engine, double sr, int buffer, int in_ch,
                  int out_ch) {
  IAudioBackend::Config cfg;
  cfg.sample_rate = sr;
  cfg.buffer_frames = buffer;
  cfg.input_channels = in_ch;
  cfg.output_channels = out_ch;
  return backend.start(cfg,
                       [&engine](const float* in, float* out, int n) { engine.process(in, out, n); });
}
}  // namespace

int main(int argc, char** argv) {
  const int record_seconds = argc > 1 ? std::atoi(argv[1]) : 6;
  const int playback_seconds = argc > 2 ? std::atoi(argv[2]) : 8;
  const double sr = 48000.0;
  const int buffer = 256;

  Engine engine;
  engine.prepare(sr, /*input_channels=*/1, /*output_channels=*/2);
  engine.setTempo(120);
  engine.setTimeSignature(4, 4);
  engine.setMetronomeEnabled(true);

  auto backend = createMiniaudioBackend();

  // --- Record: duplex device (mic in + metronome out) ---
  std::printf("Opening mic (duplex)...\n");
  if (!startBackend(*backend, engine, sr, buffer, /*in*/ 1, /*out*/ 2)) {
    std::fprintf(stderr, "Failed to open duplex device: %s\n", backend->lastError());
    return 1;
  }
  // Read reported latency while the duplex device is open.
  const double round_trip = backend->outputLatencyFrames() + backend->inputLatencyFrames();
  engine.play();
  engine.setRecording(true);
  std::printf("RECORDING %d s — play or sing along to the click!\n", record_seconds);
  std::this_thread::sleep_for(std::chrono::seconds(record_seconds));

  engine.setRecording(false);
  engine.stop();
  backend->stop();

  const bool committed = engine.commitTake(round_trip);
  std::printf("Take committed: %s | round-trip latency %.1f frames | %d track(s)\n",
              committed ? "yes" : "NO (silent / mic muted?)", round_trip, engine.trackCount());

  // --- Play back the take locked to the click ---
  if (!startBackend(*backend, engine, sr, buffer, /*in*/ 0, /*out*/ 2)) {
    std::fprintf(stderr, "Failed to reopen playback device: %s\n", backend->lastError());
    return 1;
  }
  engine.play();
  std::printf("PLAYBACK %d s — you should hear your take locked to the click.\n", playback_seconds);
  std::this_thread::sleep_for(std::chrono::seconds(playback_seconds));
  backend->stop();
  return 0;
}
