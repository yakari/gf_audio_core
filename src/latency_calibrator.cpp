// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#include "gf_audio_core/latency_calibrator.h"

#include <cmath>

namespace gf {
namespace {
constexpr double kPi = 3.14159265358979323846;
}

std::vector<float> LatencyCalibrator::makeStimulus(double sample_rate) {
  // ~10 ms exponentially-decaying 2 kHz burst: sharp onset, good autocorrelation
  // peak for delay estimation.
  const size_t n = static_cast<size_t>(sample_rate * 0.01);
  std::vector<float> s(n);
  for (size_t i = 0; i < n; ++i) {
    const double t = i / sample_rate;
    s[i] = static_cast<float>(std::sin(2.0 * kPi * 2000.0 * t) * std::exp(-t * 120.0));
  }
  return s;
}

LatencyCalibrator::Result LatencyCalibrator::estimate(const std::vector<float>& /*stimulus*/,
                                                      const std::vector<float>& /*capture*/,
                                                      double /*sample_rate*/) {
  // TODO(phase0): normalized cross-correlation of capture against stimulus;
  // return the lag of the peak (round_trip_frames) and the peak height
  // (confidence). This is the core of the calibration spike.
  return Result{};
}

}  // namespace gf
