// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#include "gf_audio_core/track.h"

namespace gf {

void Track::setBuffer(std::vector<float> samples, int channels, int64_t start_frame) {
  samples_ = std::move(samples);
  channels_ = channels > 0 ? channels : 1;
  start_frame_ = start_frame;
}

void Track::process(float* out, int out_channels, int64_t start_frame, int num_frames) const {
  if (muted_.load(std::memory_order_relaxed)) return;
  const int64_t len = lengthFrames();
  if (len <= 0) return;
  const float gain = gain_.load(std::memory_order_relaxed);

  for (int i = 0; i < num_frames; ++i) {
    const int64_t pos = start_frame + i - start_frame_;
    if (pos < 0 || pos >= len) continue;
    for (int c = 0; c < out_channels; ++c) {
      const int src_c = c < channels_ ? c : channels_ - 1;  // mono -> all out channels
      out[static_cast<size_t>(i) * out_channels + c] +=
          samples_[static_cast<size_t>(pos) * channels_ + src_c] * gain;
    }
  }
}

}  // namespace gf
