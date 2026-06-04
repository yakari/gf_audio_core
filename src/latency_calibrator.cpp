// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#include "gf_audio_core/latency_calibrator.h"

#include <cmath>
#include <vector>

namespace gf {
namespace {
constexpr double kPi = 3.14159265358979323846;

// A lag is only trusted as a real detection above this normalized-correlation
// peak. Below it the capture is probably silence/noise (wrong device, muted
// mic, headphones that don't leak into the mic, ...).
constexpr double kConfidenceThreshold = 0.5;
}  // namespace

std::vector<float> LatencyCalibrator::makeStimulus(double sample_rate) {
  // ~10 ms exponentially-decaying 2 kHz burst: sharp onset gives a clean,
  // unambiguous cross-correlation peak for delay estimation.
  const size_t n = static_cast<size_t>(sample_rate * 0.01);
  std::vector<float> s(n);
  for (size_t i = 0; i < n; ++i) {
    const double t = i / sample_rate;
    s[i] = static_cast<float>(std::sin(2.0 * kPi * 2000.0 * t) * std::exp(-t * 120.0));
  }
  return s;
}

LatencyCalibrator::Result LatencyCalibrator::estimate(const std::vector<float>& stimulus,
                                                      const std::vector<float>& capture,
                                                      double /*sample_rate*/) {
  Result result;
  const size_t m = stimulus.size();
  const size_t n = capture.size();
  if (m < 2 || n < m) return result;  // not enough data to slide the stimulus

  // Energy of the stimulus is constant across lags; precompute it once.
  double stimulus_energy = 0.0;
  for (float v : stimulus) stimulus_energy += static_cast<double>(v) * v;
  if (stimulus_energy <= 0.0) return result;

  // Slide the stimulus over the capture and compute the *normalized* cross-
  // correlation at every integer lag. Normalizing (dividing by the geometric
  // mean of the two energies) makes the peak amplitude-independent, so the
  // confidence is a clean 0..1 score regardless of input gain.
  const size_t max_lag = n - m;
  std::vector<double> ncc(max_lag + 1, 0.0);
  for (size_t lag = 0; lag <= max_lag; ++lag) {
    const float* window = &capture[lag];
    double dot = 0.0;
    double window_energy = 0.0;
    for (size_t i = 0; i < m; ++i) {
      dot += static_cast<double>(stimulus[i]) * window[i];
      window_energy += static_cast<double>(window[i]) * window[i];
    }
    const double denom = std::sqrt(stimulus_energy * window_energy);
    ncc[lag] = denom > 1e-12 ? dot / denom : 0.0;
  }

  // Locate the peak lag = the round-trip delay in whole frames.
  size_t peak = 0;
  double peak_value = ncc[0];
  for (size_t lag = 1; lag <= max_lag; ++lag) {
    if (ncc[lag] > peak_value) {
      peak_value = ncc[lag];
      peak = lag;
    }
  }

  // Sub-sample refinement: fit a parabola through the peak and its neighbours
  // to recover a fractional-frame delay (real latency rarely lands on an exact
  // sample boundary).
  double refined = static_cast<double>(peak);
  if (peak > 0 && peak < max_lag) {
    const double y0 = ncc[peak - 1];
    const double y1 = ncc[peak];
    const double y2 = ncc[peak + 1];
    const double curvature = y0 - 2.0 * y1 + y2;
    if (std::fabs(curvature) > 1e-12) {
      const double offset = 0.5 * (y0 - y2) / curvature;
      if (offset > -1.0 && offset < 1.0) refined = static_cast<double>(peak) + offset;
    }
  }

  result.valid = peak_value >= kConfidenceThreshold;
  result.round_trip_frames = refined;
  result.confidence = peak_value;
  return result;
}

}  // namespace gf
