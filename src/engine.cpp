// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#include "gf_audio_core/engine.h"

#include <cstring>

namespace gf {

void Engine::prepare(double sample_rate, int input_channels, int output_channels) {
  input_channels_ = input_channels > 0 ? input_channels : 0;
  output_channels_ = output_channels > 0 ? output_channels : 2;

  transport_.setSampleRate(sample_rate);
  transport_.setTempo(target_bpm_.load(std::memory_order_relaxed));
  transport_.setTimeSignature(
      {target_ts_num_.load(std::memory_order_relaxed), target_ts_den_.load(std::memory_order_relaxed)});
  metronome_.prepare(sample_rate);

  // ~8 s of input capture headroom between record-thread drains.
  const int in_ch = input_channels_ > 0 ? input_channels_ : 1;
  record_ring_.reset(static_cast<size_t>(sample_rate * in_ch * 8.0));

  head_ = 0;
  play_head_.store(0, std::memory_order_relaxed);
}

void Engine::setTimeSignature(int numerator, int denominator) {
  target_ts_num_.store(numerator, std::memory_order_relaxed);
  target_ts_den_.store(denominator, std::memory_order_relaxed);
}

void Engine::play() {
  seek_to_.store(0, std::memory_order_relaxed);
  playing_.store(true, std::memory_order_relaxed);
}

void Engine::stop() { playing_.store(false, std::memory_order_relaxed); }

Track* Engine::addTrack() {
  const int c = track_count_.load(std::memory_order_relaxed);
  if (c >= kMaxTracks) return nullptr;
  track_count_.store(c + 1, std::memory_order_release);
  return &tracks_[c];
}

void Engine::process(const float* in, float* out, int num_frames) {
  const int out_ch = output_channels_;

  // Apply control-thread changes at the block boundary (cheap, no allocation).
  const double bpm = target_bpm_.load(std::memory_order_relaxed);
  if (bpm != transport_.tempo()) transport_.setTempo(bpm);
  const int tsn = target_ts_num_.load(std::memory_order_relaxed);
  const int tsd = target_ts_den_.load(std::memory_order_relaxed);
  if (tsn != transport_.timeSignature().numerator || tsd != transport_.timeSignature().denominator)
    transport_.setTimeSignature({tsn, tsd});

  const int64_t seek = seek_to_.load(std::memory_order_relaxed);
  if (seek >= 0) {
    head_ = seek;
    metronome_.reset();
    seek_to_.store(-1, std::memory_order_relaxed);
  }

  std::memset(out, 0, sizeof(float) * static_cast<size_t>(num_frames) * out_ch);

  if (metronome_on_.load(std::memory_order_relaxed))
    metronome_.process(out, out_ch, head_, num_frames, transport_);

  if (playing_.load(std::memory_order_relaxed)) {
    const int count = track_count_.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) tracks_[i].process(out, out_ch, head_, num_frames);

    if (in != nullptr && input_channels_ > 0 && recording_.load(std::memory_order_relaxed))
      record_ring_.push(in, static_cast<size_t>(num_frames) * input_channels_);

    head_ += num_frames;
    play_head_.store(head_, std::memory_order_relaxed);
  }

  // Master clip guard.
  const size_t total = static_cast<size_t>(num_frames) * out_ch;
  for (size_t i = 0; i < total; ++i) {
    const float s = out[i];
    out[i] = s > 1.0f ? 1.0f : (s < -1.0f ? -1.0f : s);
  }
}

}  // namespace gf
