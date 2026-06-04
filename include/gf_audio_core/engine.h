// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#pragma once
#include <array>
#include <atomic>
#include <cstdint>

#include "gf_audio_core/dsp/ring_buffer.h"
#include "gf_audio_core/metronome.h"
#include "gf_audio_core/track.h"
#include "gf_audio_core/transport.h"

namespace gf {

// The realtime mixer/clock. process() is the audio callback: it advances the
// transport, renders the metronome and tracks, and captures input for
// recording. It must never allocate, lock, or block.
//
// Control-thread methods (setTempo, play, etc.) communicate with the audio
// thread only through std::atomic. Adding tracks is currently allowed only
// while stopped (a lock-free command queue for live edits is a TODO).
class Engine {
 public:
  Engine() = default;

  // Allocates buffers and pre-renders the metronome. Call before starting the
  // backend; NOT RT-safe.
  void prepare(double sample_rate, int input_channels, int output_channels);

  // ---- control thread ----
  void setTempo(double bpm) { target_bpm_.store(bpm, std::memory_order_relaxed); }
  void setTimeSignature(int numerator, int denominator);
  void setMetronomeEnabled(bool on) { metronome_on_.store(on, std::memory_order_relaxed); }
  void play();   // seek to 0 and start the transport
  void stop();   // pause the transport (keeps playhead)
  // Enabling recording re-arms the start-head latch; the audio thread stamps
  // the playhead at the first captured block (see recordStartHead()).
  void setRecording(bool on) {
    if (on) record_start_head_.store(-1, std::memory_order_release);
    recording_.store(on, std::memory_order_release);
  }

  // Grid frame at which the current/last capture began, or -1 if capture has
  // not produced a block yet. The recorded take must be shifted earlier by the
  // calibrated round-trip latency relative to this origin (see recorder.h).
  int64_t recordStartHead() const { return record_start_head_.load(std::memory_order_acquire); }

  // Pre-start only.
  Track* addTrack();

  int64_t playHeadFrames() const { return play_head_.load(std::memory_order_relaxed); }

  // Drains captured input frames (consumer side of the record ring). Call from
  // a non-audio thread while recording. Returns frames written into `out`.
  size_t drainCapturedInput(float* out, size_t max_samples) {
    return record_ring_.pop(out, max_samples);
  }

  // ---- audio thread ----
  void process(const float* in, float* out, int num_frames);

 private:
  static constexpr int kMaxTracks = 16;

  Transport transport_;
  Metronome metronome_;
  std::array<Track, kMaxTracks> tracks_;  // fixed slots; unused ones are silent
  std::atomic<int> track_count_{0};
  dsp::SpscRingBuffer<float> record_ring_;

  int input_channels_ = 1;
  int output_channels_ = 2;

  std::atomic<double> target_bpm_{120.0};
  std::atomic<int> target_ts_num_{4};
  std::atomic<int> target_ts_den_{4};
  std::atomic<bool> metronome_on_{true};
  std::atomic<bool> playing_{false};
  std::atomic<bool> recording_{false};
  std::atomic<int64_t> record_start_head_{-1};  // playhead latched at capture start
  std::atomic<int64_t> seek_to_{-1};            // >=0 requests a playhead seek
  std::atomic<int64_t> play_head_{0};

  int64_t head_ = 0;  // audio-thread-owned mirror of the playhead
};

}  // namespace gf
