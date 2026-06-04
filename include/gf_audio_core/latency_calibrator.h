// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#pragma once
#include <cstdint>
#include <vector>

namespace gf {

// Measures full round-trip (output -> air/wire -> input) latency so recorded
// takes can be shifted back onto the grid. The core algorithm: emit a known
// click, capture it via the mic, cross-correlate to find the delay.
//
// This is the make-or-break of the whole product (esp. over Bluetooth) and is
// the primary target of the Phase 0 spike. This header defines the contract;
// the implementation is a STUB for now.
class LatencyCalibrator {
 public:
  struct Result {
    bool valid = false;
    double round_trip_frames = 0.0;  // measured output+input latency in frames
    double confidence = 0.0;         // 0..1 cross-correlation peak quality
  };

  // Returns the stimulus (a click burst) to play out during calibration.
  static std::vector<float> makeStimulus(double sample_rate);

  // Cross-correlates the recorded capture against the stimulus to estimate the
  // round-trip delay. TODO(phase0): implement.
  static Result estimate(const std::vector<float>& stimulus,
                         const std::vector<float>& capture, double sample_rate);
};

}  // namespace gf
