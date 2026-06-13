// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#include "gf_audio_core/engine.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "gf_audio_core/recorder.h"

namespace gf {
namespace {
// v1 take-length cap: seconds of capture held in the ring until commit. Longer
// takes need a streaming drain (TODO); this keeps the always-allocated ring small.
constexpr double kCaptureSeconds = 30.0;
}  // namespace

void Engine::prepare(double sample_rate, int input_channels, int output_channels) {
  input_channels_ = input_channels > 0 ? input_channels : 0;
  output_channels_ = output_channels > 0 ? output_channels : 2;

  transport_.setSampleRate(sample_rate);
  transport_.setTempo(target_bpm_.load(std::memory_order_relaxed));
  transport_.setTimeSignature(
      {target_ts_num_.load(std::memory_order_relaxed), target_ts_den_.load(std::memory_order_relaxed)});
  metronome_.prepare(sample_rate);

  // Capture buffer for one take, drained once at commit (see commitTake).
  const int in_ch = input_channels_ > 0 ? input_channels_ : 1;
  record_ring_.reset(static_cast<size_t>(sample_rate * in_ch * kCaptureSeconds));

  head_ = 0;
  play_head_.store(0, std::memory_order_relaxed);
}

void Engine::setTimeSignature(int numerator, int denominator) {
  target_ts_num_.store(numerator, std::memory_order_relaxed);
  target_ts_den_.store(denominator, std::memory_order_relaxed);
}

void Engine::seekTo(int64_t frame) {
  seek_target_.store(frame, std::memory_order_relaxed);
  seek_pending_.store(true, std::memory_order_release);
}

double Engine::framesPerBarNow() const {
  const double bpm = target_bpm_.load(std::memory_order_relaxed);
  const int num = target_ts_num_.load(std::memory_order_relaxed);
  const int den = target_ts_den_.load(std::memory_order_relaxed);
  if (bpm <= 0.0 || den <= 0) return 0.0;
  const double frames_per_quarter = transport_.sampleRate() * 60.0 / bpm;
  return frames_per_quarter * 4.0 / den * num;
}

void Engine::play() {
  seekTo(0);
  playing_.store(true, std::memory_order_relaxed);
}

void Engine::stop() { playing_.store(false, std::memory_order_relaxed); }

void Engine::startTake(int count_in_bars) {
  const int bars = count_in_bars > 0 ? count_in_bars : 0;
  const int64_t count_in = static_cast<int64_t>(std::llround(bars * framesPerBarNow()));
  record_start_head_.store(-1, std::memory_order_relaxed);
  recording_.store(true, std::memory_order_release);
  seekTo(-count_in);  // start the playhead before bar 0 so the count-in plays
  playing_.store(true, std::memory_order_relaxed);
}

Track* Engine::addTrack() {
  const int c = track_count_.load(std::memory_order_relaxed);
  if (c >= kMaxTracks) return nullptr;
  track_count_.store(c + 1, std::memory_order_release);
  return &tracks_[c];
}

void Engine::setTrackMuted(int index, bool muted) {
  if (index >= 0 && index < track_count_.load(std::memory_order_acquire))
    tracks_[index].setMuted(muted);
}

void Engine::setTrackGain(int index, float gain) {
  if (index >= 0 && index < track_count_.load(std::memory_order_acquire))
    tracks_[index].setGain(gain);
}

int Engine::trackFrameCount(int index) const {
  if (index < 0 || index >= track_count_.load(std::memory_order_acquire)) return 0;
  return static_cast<int>(tracks_[index].lengthFrames());
}

int Engine::trackChannels(int index) const {
  if (index < 0 || index >= track_count_.load(std::memory_order_acquire)) return 0;
  return tracks_[index].channels();
}

int64_t Engine::trackStartFrame(int index) const {
  if (index < 0 || index >= track_count_.load(std::memory_order_acquire)) return 0;
  return tracks_[index].startFrame();
}

int Engine::copyTrackSamples(int index, float* out, int max_samples) const {
  if (index < 0 || index >= track_count_.load(std::memory_order_acquire)) return 0;
  return tracks_[index].copyInto(out, max_samples);
}

int Engine::addTrackData(const float* samples, int sample_count, int channels, int64_t start_frame) {
  const int c = track_count_.load(std::memory_order_relaxed);
  if (c >= kMaxTracks || sample_count <= 0 || samples == nullptr) return -1;
  std::vector<float> buffer(samples, samples + sample_count);
  tracks_[c].setBuffer(std::move(buffer), channels, start_frame);  // fill the slot first
  track_count_.store(c + 1, std::memory_order_release);            // then publish
  return c;
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

  if (seek_pending_.exchange(false, std::memory_order_acquire)) {
    head_ = seek_target_.load(std::memory_order_relaxed);
    metronome_.reset();
  }

  std::memset(out, 0, sizeof(float) * static_cast<size_t>(num_frames) * out_ch);

  if (metronome_on_.load(std::memory_order_relaxed))
    metronome_.process(out, out_ch, head_, num_frames, transport_);

  if (playing_.load(std::memory_order_relaxed)) {
    const int count = track_count_.load(std::memory_order_acquire);
    for (int i = 0; i < count; ++i) tracks_[i].process(out, out_ch, head_, num_frames);

    // Capture only once we reach bar 0 (head >= 0): the count-in plays the
    // metronome at negative head but is not recorded.
    if (in != nullptr && input_channels_ > 0 && head_ >= 0 &&
        recording_.load(std::memory_order_relaxed)) {
      // Stamp the grid position of the first captured frame so the take can be
      // aligned (latency-compensated) back onto the grid.
      if (record_start_head_.load(std::memory_order_relaxed) < 0)
        record_start_head_.store(head_, std::memory_order_release);
      record_ring_.push(in, static_cast<size_t>(num_frames) * input_channels_);
    }

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

bool Engine::commitTake(double round_trip_frames) {
  // Drain everything captured during the take. Safe only with the audio device
  // stopped — no concurrent producer on the ring.
  std::vector<float> captured;
  float tmp[4096];
  size_t n;
  while ((n = record_ring_.pop(tmp, 4096)) > 0) captured.insert(captured.end(), tmp, tmp + n);
  if (captured.empty()) return false;

  AlignedTake take = alignCapturedTake(std::move(captured), input_channels_,
                                       record_start_head_.load(std::memory_order_relaxed),
                                       round_trip_frames);
  if (take.samples.empty()) return false;

  Track* track = addTrack();
  if (track == nullptr) return false;
  track->setBuffer(std::move(take.samples), take.channels, take.start_frame);

  record_start_head_.store(-1, std::memory_order_relaxed);
  return true;
}

}  // namespace gf
