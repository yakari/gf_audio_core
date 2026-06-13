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

  // Starts a recording take with an optional count-in: the metronome ticks for
  // count_in_bars bars before bar 0, then existing tracks play and capture
  // begins at bar 0. count_in_bars == 0 records immediately from bar 0.
  void startTake(int count_in_bars);

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

  // Drains all captured input and commits it as a grid-aligned, playable track,
  // shifted earlier by round_trip_frames to undo monitoring latency. Call ONLY
  // with the audio device stopped (it mutates the track list and drains the
  // ring). Returns true if a take was added.
  bool commitTake(double round_trip_frames);

  int trackCount() const { return track_count_.load(std::memory_order_acquire); }

  // Per-track mix controls (RT-safe; index must be in [0, trackCount())).
  void setTrackMuted(int index, bool muted);
  void setTrackGain(int index, float gain);  // linear, 1.0 = unity

  // ---- take export / import (for LAN transfer) ----
  // Export a recorded track's PCM + grid position (RT-safe reads, ok while
  // playing). copyTrackSamples fills `out` (size >= frameCount * channels) and
  // returns the count copied.
  int trackFrameCount(int index) const;
  int trackChannels(int index) const;
  int64_t trackStartFrame(int index) const;
  int copyTrackSamples(int index, float* out, int max_samples) const;

  // Import external PCM as a track placed at start_frame. Thread-safe while
  // playing (fills the slot, then publishes the count). Returns the new track
  // index, or -1 if full / empty.
  int addTrackData(const float* samples, int sample_count, int channels, int64_t start_frame);

  // ---- audio thread ----
  void process(const float* in, float* out, int num_frames);

 private:
  static constexpr int kMaxTracks = 16;

  // Frames per bar from the current target tempo / time signature (control
  // thread); used to size the count-in.
  double framesPerBarNow() const;
  void seekTo(int64_t frame);

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
  std::atomic<bool> seek_pending_{false};       // a seek to seek_target_ is requested
  std::atomic<int64_t> seek_target_{0};         // target playhead frame (may be negative)
  std::atomic<int64_t> play_head_{0};

  int64_t head_ = 0;  // audio-thread-owned mirror of the playhead
};

}  // namespace gf
