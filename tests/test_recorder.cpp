// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#include <cmath>
#include <vector>

#include "check.h"
#include "gf_audio_core/recorder.h"

using namespace gf;

// Frame index of the first impulse (|sample| > 0.5) in a mono buffer, or -1.
static int64_t firstImpulse(const std::vector<float>& v) {
  for (size_t i = 0; i < v.size(); ++i)
    if (std::fabs(v[i]) > 0.5f) return static_cast<int64_t>(i);
  return -1;
}

int main() {
  // The player hit beats on grid 0 and 24000, but the engine captured them late
  // by a round-trip of L frames, with capture starting at grid head 0. So the
  // impulses appear in the raw capture at L and 24000 + L.
  const int64_t kL = 3000;
  const int64_t kRecordStartHead = 0;
  std::vector<float> captured(24000 + kL + 1000, 0.0f);
  captured[kL] = 1.0f;
  captured[24000 + kL] = 1.0f;

  const AlignedTake take =
      alignCapturedTake(captured, /*channels=*/1, kRecordStartHead, static_cast<double>(kL));

  // ideal_start = 0 - L = -L  ->  trim L leading frames, clamp start to grid 0.
  CHECK(take.start_frame == 0);
  CHECK(take.trimmed_frames == kL);
  // After compensation the take's impulses land exactly on grid 0 and 24000.
  CHECK(firstImpulse(take.samples) == 0);
  CHECK(take.samples.size() > 24000);
  CHECK(std::fabs(take.samples[24000]) > 0.5f);

  // Capture started mid-grid with latency small enough to keep start positive:
  // no trimming, samples untouched, origin shifted earlier by the latency.
  const int64_t kHead2 = 10000;
  const double kL2 = 2500.0;
  std::vector<float> cap2(5000, 0.0f);
  cap2[100] = 1.0f;
  const AlignedTake t2 = alignCapturedTake(cap2, 1, kHead2, kL2);
  CHECK(t2.trimmed_frames == 0);
  CHECK(t2.start_frame == kHead2 - 2500);  // 7500
  CHECK(std::fabs(t2.samples[100]) > 0.5f);

  if (g_failures == 0) std::printf("recorder: OK\n");
  return g_failures;
}
