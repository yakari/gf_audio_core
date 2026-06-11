// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#pragma once
#include <cstdint>
#include <vector>

#include "gf_audio_core/transport.h"

namespace gf {

// Generates click sounds on the beat grid. The click waveforms are pre-rendered
// in prepare(); process() only reads from them, so it is RT-safe (no allocation,
// no locks). Accent (downbeat) and normal clicks are distinguished by pitch.
class Metronome {
 public:
  void prepare(double sample_rate);  // pre-renders clicks; NOT RT-safe
  void reset();                      // clears playback state (call on seek)

  // Mixes clicks for the block [start_frame, start_frame + num_frames) into the
  // interleaved `out` buffer. Additive: caller must have zeroed/own the mix.
  void process(float* out, int out_channels, int64_t start_frame, int num_frames,
               const Transport& transport);

 private:
  std::vector<float> accent_click_;
  std::vector<float> normal_click_;
  const std::vector<float>* active_ = nullptr;
  size_t active_pos_ = 0;
  int64_t last_beat_index_ = INT64_MIN;
};

}  // namespace gf
