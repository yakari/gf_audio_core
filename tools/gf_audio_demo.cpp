// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

// Minimal smoke test of the toolchain + audio output: plays the metronome on
// the default device. Proves the engine renders audio end-to-end on this box.
//
//   ./gf_audio_demo [bpm] [seconds]
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "gf_audio_core/audio_backend.h"
#include "gf_audio_core/engine.h"

int main(int argc, char** argv) {
  const double bpm = argc > 1 ? std::atof(argv[1]) : 120.0;
  const int seconds = argc > 2 ? std::atoi(argv[2]) : 8;
  const double sr = 48000.0;
  const int buffer_frames = 256;

  gf::Engine engine;
  engine.prepare(sr, /*input_channels=*/1, /*output_channels=*/2);
  engine.setTempo(bpm);
  engine.setTimeSignature(4, 4);
  engine.setMetronomeEnabled(true);
  engine.play();

  auto backend = gf::createMiniaudioBackend();
  gf::IAudioBackend::Config cfg;
  cfg.sample_rate = sr;
  cfg.buffer_frames = buffer_frames;
  cfg.input_channels = 1;
  cfg.output_channels = 2;

  if (!backend->start(cfg, [&](const float* in, float* out, int n) { engine.process(in, out, n); })) {
    std::fprintf(stderr, "Failed to start audio backend\n");
    return 1;
  }

  std::printf("Metronome @ %.0f BPM, 4/4 — playing for %d s...\n", bpm, seconds);
  std::this_thread::sleep_for(std::chrono::seconds(seconds));
  backend->stop();
  return 0;
}
