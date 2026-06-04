// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#include <cmath>
#include <vector>

#include "check.h"
#include "gf_audio_core/latency_calibrator.h"

using namespace gf;

int main() {
  const double sr = 48000.0;
  const std::vector<float> stim = LatencyCalibrator::makeStimulus(sr);
  CHECK(!stim.empty());

  // Synthesize a capture: silence with the stimulus embedded at a known delay,
  // attenuated (room/gain) plus a little deterministic low-level "noise".
  const int delay = 1234;
  std::vector<float> capture(delay + stim.size() + 2000, 0.0f);
  for (size_t i = 0; i < stim.size(); ++i) capture[delay + i] += stim[i] * 0.3f;
  for (size_t i = 0; i < capture.size(); ++i)
    capture[i] += 0.001f * static_cast<float>(std::sin(0.07 * i));

  const LatencyCalibrator::Result r = LatencyCalibrator::estimate(stim, capture, sr);
  CHECK(r.valid);
  CHECK(r.confidence > 0.8);
  CHECK_NEAR(r.round_trip_frames, static_cast<double>(delay), 1.0);

  // A capture shorter than the stimulus cannot yield a measurement.
  const std::vector<float> tiny(stim.size() / 2, 0.0f);
  const LatencyCalibrator::Result r2 = LatencyCalibrator::estimate(stim, tiny, sr);
  CHECK(!r2.valid);

  // Pure silence -> no confident detection.
  const std::vector<float> silence(stim.size() + 1000, 0.0f);
  const LatencyCalibrator::Result r3 = LatencyCalibrator::estimate(stim, silence, sr);
  CHECK(!r3.valid);

  if (g_failures == 0) std::printf("latency_calibrator: OK\n");
  return g_failures;
}
