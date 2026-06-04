// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#pragma once
#include <cstdint>

namespace gf {

struct TimeSignature {
  int numerator = 4;    // beats per bar
  int denominator = 4;  // note value that gets one beat (4 = quarter, 8 = eighth)
};

struct BarBeat {
  int64_t bar = 0;       // 0-based bar index
  int beat = 0;          // 0-based beat within the bar
  double fraction = 0.0; // fractional position within the beat, [0, 1)
};

// Musical clock: converts between sample frames and bars/beats.
//
// Tempo is expressed in quarter-notes per minute (the usual "BPM"). A "beat"
// for grid/metronome purposes is one note of the time-signature denominator,
// so in 6/8 there are 6 eighth-note beats per bar.
//
// Mutation is NOT thread-safe; the audio thread reads the cached frame counts
// after the control thread has applied changes (see Engine for the handoff).
class Transport {
 public:
  void setSampleRate(double sr);
  void setTempo(double bpm);
  void setTimeSignature(TimeSignature ts);

  double sampleRate() const { return sample_rate_; }
  double tempo() const { return bpm_; }
  TimeSignature timeSignature() const { return ts_; }
  int beatsPerBar() const { return ts_.numerator; }

  double framesPerQuarter() const { return frames_per_quarter_; }
  double framesPerBeat() const;  // length of one notated beat
  double framesPerBar() const;   // length of a full bar

  int64_t frameForBar(int64_t bar) const;
  int64_t frameForBeat(int64_t absolute_beat) const;
  BarBeat barBeatForFrame(int64_t frame) const;

 private:
  void recompute();
  double sample_rate_ = 48000.0;
  double bpm_ = 120.0;
  TimeSignature ts_{};
  double frames_per_quarter_ = 0.0;
};

}  // namespace gf
