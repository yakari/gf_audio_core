// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#include "gf_audio_core/transport.h"

#include "check.h"

using namespace gf;

int main() {
  Transport t;
  t.setSampleRate(48000.0);
  t.setTempo(120.0);
  t.setTimeSignature({4, 4});

  // 120 BPM quarter note = 0.5 s = 24000 frames @ 48 kHz.
  CHECK_NEAR(t.framesPerQuarter(), 24000.0, 1e-6);
  CHECK_NEAR(t.framesPerBeat(), 24000.0, 1e-6);
  CHECK_NEAR(t.framesPerBar(), 96000.0, 1e-6);
  CHECK(t.frameForBar(1) == 96000);

  BarBeat bb = t.barBeatForFrame(96000);
  CHECK(bb.bar == 1);
  CHECK(bb.beat == 0);
  CHECK_NEAR(bb.fraction, 0.0, 1e-9);

  // Halfway through beat 1 (the 2nd beat) of bar 0.
  BarBeat mid = t.barBeatForFrame(24000 + 12000);
  CHECK(mid.bar == 0);
  CHECK(mid.beat == 1);
  CHECK_NEAR(mid.fraction, 0.5, 1e-9);

  // 6/8: beat = eighth note = half a quarter = 12000 frames; bar = 6 beats.
  t.setTimeSignature({6, 8});
  CHECK_NEAR(t.framesPerBeat(), 12000.0, 1e-6);
  CHECK_NEAR(t.framesPerBar(), 72000.0, 1e-6);
  CHECK(t.beatsPerBar() == 6);

  if (g_failures == 0) std::printf("transport: OK\n");
  return g_failures;
}
