// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#include "gf_audio_core/recorder.h"

#include <cmath>
#include <utility>

namespace gf {

AlignedTake alignCapturedTake(std::vector<float> captured, int channels,
                              int64_t record_start_head, double round_trip_frames) {
  AlignedTake take;
  take.channels = channels > 0 ? channels : 1;

  // Where the take *should* sit on the grid once we undo the round-trip delay.
  const int64_t latency = static_cast<int64_t>(std::llround(round_trip_frames));
  int64_t ideal_start = record_start_head - latency;

  if (ideal_start < 0) {
    // The first (-ideal_start) frames would sit before bar 0 — drop them.
    take.trimmed_frames = -ideal_start;
    const size_t drop_samples = static_cast<size_t>(take.trimmed_frames) * take.channels;
    if (drop_samples >= captured.size()) {
      take.start_frame = 0;  // whole take is before the grid origin -> empty
      return take;
    }
    captured.erase(captured.begin(), captured.begin() + static_cast<std::ptrdiff_t>(drop_samples));
    ideal_start = 0;
  }

  take.start_frame = ideal_start;
  take.samples = std::move(captured);
  return take;
}

}  // namespace gf
