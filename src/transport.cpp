// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#include "gf_audio_core/transport.h"

#include <cmath>

namespace gf {

void Transport::setSampleRate(double sr) {
  sample_rate_ = sr;
  recompute();
}
void Transport::setTempo(double bpm) {
  bpm_ = bpm;
  recompute();
}
void Transport::setTimeSignature(TimeSignature ts) {
  ts_ = ts;
  recompute();
}

void Transport::recompute() {
  frames_per_quarter_ = (bpm_ > 0.0) ? sample_rate_ * 60.0 / bpm_ : 0.0;
}

double Transport::framesPerBeat() const {
  return ts_.denominator > 0 ? frames_per_quarter_ * 4.0 / ts_.denominator : 0.0;
}
double Transport::framesPerBar() const { return framesPerBeat() * ts_.numerator; }

int64_t Transport::frameForBar(int64_t bar) const {
  return static_cast<int64_t>(std::llround(bar * framesPerBar()));
}
int64_t Transport::frameForBeat(int64_t absolute_beat) const {
  return static_cast<int64_t>(std::llround(absolute_beat * framesPerBeat()));
}

BarBeat Transport::barBeatForFrame(int64_t frame) const {
  BarBeat r;
  const double fpb = framesPerBeat();
  if (fpb <= 0.0) return r;
  const double total_beats = frame / fpb;
  const int64_t abs_beat = static_cast<int64_t>(std::floor(total_beats));
  const int num = ts_.numerator > 0 ? ts_.numerator : 1;
  r.bar = abs_beat / num;
  r.beat = static_cast<int>(abs_beat - r.bar * num);
  r.fraction = total_beats - static_cast<double>(abs_beat);
  return r;
}

}  // namespace gf
