// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#pragma once
#include <cstdint>
#include <vector>

namespace gf {

// A captured take placed onto the shared grid after latency compensation.
struct AlignedTake {
  std::vector<float> samples;  // interleaved, `channels` channels
  int channels = 1;
  int64_t start_frame = 0;     // grid frame where samples[0] belongs (>= 0)
  int64_t trimmed_frames = 0;  // leading frames dropped because they fell before grid 0
};

// Shifts a freshly captured take earlier by the measured round-trip latency so
// it lines up with what the player actually heard:
//
//   ideal_start = record_start_head - round_trip_frames
//
// `record_start_head` is the grid frame at which capture began
// (Engine::recordStartHead()); `round_trip_frames` is the calibrated
// output+input latency (LatencyCalibrator::Result::round_trip_frames).
//
// If the ideal start lands before grid 0, the leading frames are trimmed and
// start_frame is clamped to 0 (the drop count is reported in trimmed_frames).
//
// Rounds to whole-frame precision. Sub-sample (fractional-delay) shifting is a
// future refinement; at 48 kHz one frame is ~21 us, far below audible overdub
// timing error.
AlignedTake alignCapturedTake(std::vector<float> captured, int channels,
                              int64_t record_start_head, double round_trip_frames);

}  // namespace gf
