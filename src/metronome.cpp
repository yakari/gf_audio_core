// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#include "gf_audio_core/metronome.h"

#include <cmath>

namespace gf {
namespace {
constexpr double kPi = 3.14159265358979323846;

std::vector<float> renderClick(double sr, double freq, double dur, float amp) {
  const size_t n = static_cast<size_t>(sr * dur);
  std::vector<float> buf(n);
  const double decay = 25.0;  // exponential amplitude decay rate
  for (size_t i = 0; i < n; ++i) {
    const double t = i / sr;
    buf[i] = static_cast<float>(std::sin(2.0 * kPi * freq * t) * std::exp(-t * decay)) * amp;
  }
  return buf;
}
}  // namespace

void Metronome::prepare(double sr) {
  accent_click_ = renderClick(sr, 1500.0, 0.05, 0.9f);  // downbeat: higher
  normal_click_ = renderClick(sr, 1000.0, 0.04, 0.7f);
  reset();
}

void Metronome::reset() {
  active_ = nullptr;
  active_pos_ = 0;
  last_beat_index_ = INT64_MIN;  // sentinel that no beat index can collide with
}

void Metronome::process(float* out, int out_channels, int64_t start_frame, int num_frames,
                        const Transport& transport) {
  const double fpb = transport.framesPerBeat();
  int num = transport.beatsPerBar();
  if (num < 1) num = 1;

  for (int i = 0; i < num_frames; ++i) {
    const int64_t f = start_frame + i;
    if (fpb > 0.0) {  // also ticks at negative f for the count-in before bar 0
      const int64_t beat_index = static_cast<int64_t>(std::floor(f / fpb));
      if (beat_index != last_beat_index_) {
        last_beat_index_ = beat_index;
        const bool accent = (beat_index % num) == 0;
        active_ = accent ? &accent_click_ : &normal_click_;
        active_pos_ = 0;
      }
    }
    float s = 0.0f;
    if (active_ != nullptr && active_pos_ < active_->size()) {
      s = (*active_)[active_pos_++];
      if (active_pos_ >= active_->size()) active_ = nullptr;
    }
    for (int c = 0; c < out_channels; ++c) out[static_cast<size_t>(i) * out_channels + c] += s;
  }
}

}  // namespace gf
