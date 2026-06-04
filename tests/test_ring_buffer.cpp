// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#include "gf_audio_core/dsp/ring_buffer.h"

#include "check.h"

using gf::dsp::SpscRingBuffer;

int main() {
  SpscRingBuffer<float> rb(8);  // already a power of two
  CHECK(rb.capacity() == 8);
  CHECK(rb.available() == 0);

  float in[5] = {1, 2, 3, 4, 5};
  CHECK(rb.push(in, 5) == 5);
  CHECK(rb.available() == 5);

  float out[5] = {0};
  CHECK(rb.pop(out, 5) == 5);
  for (int i = 0; i < 5; ++i) CHECK(out[i] == in[i]);
  CHECK(rb.available() == 0);

  // Overfill: capacity 8, pushing 10 writes only 8 (head/tail are now offset,
  // so this also exercises index wrap-around).
  float big[10];
  for (int i = 0; i < 10; ++i) big[i] = static_cast<float>(i);
  CHECK(rb.push(big, 10) == 8);
  CHECK(rb.available() == 8);

  float tmp[4];
  CHECK(rb.pop(tmp, 4) == 4);
  for (int i = 0; i < 4; ++i) CHECK(tmp[i] == big[i]);
  CHECK(rb.available() == 4);

  float more[4] = {100, 101, 102, 103};
  CHECK(rb.push(more, 4) == 4);
  CHECK(rb.available() == 8);

  // Capacity rounding: 5 -> 8.
  SpscRingBuffer<int> rb2(5);
  CHECK(rb2.capacity() == 8);

  if (g_failures == 0) std::printf("ring_buffer: OK\n");
  return g_failures;
}
