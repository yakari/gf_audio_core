// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#pragma once
#include <atomic>
#include <cstdint>
#include <vector>

namespace gf {

// A single recorded take, aligned to the shared timeline. Holds interleaved PCM
// and plays back starting at `start_frame` on the global grid.
//
// setBuffer() mutates the sample store and is NOT RT-safe: load takes while the
// stream is stopped, or hand them to the engine via a command queue (TODO).
// process(), setGain() and setMuted() are RT-safe.
class Track {
 public:
  void setBuffer(std::vector<float> samples, int channels, int64_t start_frame);

  void setGain(float gain) { gain_.store(gain, std::memory_order_relaxed); }
  void setMuted(bool muted) { muted_.store(muted, std::memory_order_relaxed); }
  bool muted() const { return muted_.load(std::memory_order_relaxed); }
  int64_t startFrame() const { return start_frame_; }
  int64_t lengthFrames() const {
    return channels_ > 0 ? static_cast<int64_t>(samples_.size() / channels_) : 0;
  }

  // Mixes this track's audio for [start_frame, start_frame + num_frames) into
  // the interleaved `out` buffer (additive). RT-safe.
  void process(float* out, int out_channels, int64_t start_frame, int num_frames) const;

 private:
  std::vector<float> samples_;  // interleaved
  int channels_ = 1;
  int64_t start_frame_ = 0;
  std::atomic<float> gain_{1.0f};
  std::atomic<bool> muted_{false};
};

}  // namespace gf
