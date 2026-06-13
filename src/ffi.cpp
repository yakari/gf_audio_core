// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#include "gf_audio_core/gf_audio_core.h"

#include <memory>

#include "gf_audio_core/audio_backend.h"
#include "gf_audio_core/engine.h"

struct gf_engine {
  gf::Engine engine;
  std::unique_ptr<gf::IAudioBackend> backend;
  double sample_rate = 48000.0;
  int buffer_frames = 256;
  int output_channels = 2;
  int capture_channels = 1;  // channels captured during a take
  bool recording = false;
};

namespace {
// (Re)opens the audio device in the given input mode (0 = playback-only,
// >0 = duplex, which acquires the mic). Recreates the render closure each time.
// Returns 1 on success.
int gfStartBackend(gf_engine* e, int input_channels) {
  if (e->backend && e->backend->isRunning()) e->backend->stop();
  if (!e->backend) e->backend = gf::createMiniaudioBackend();
  gf::IAudioBackend::Config cfg;
  cfg.sample_rate = e->sample_rate;
  cfg.buffer_frames = e->buffer_frames;
  cfg.input_channels = input_channels;
  cfg.output_channels = e->output_channels;
  gf::Engine* engine = &e->engine;
  return e->backend->start(cfg, [engine](const float* in, float* out, int n) {
    engine->process(in, out, n);
  })
             ? 1
             : 0;
}
}  // namespace

extern "C" {

gf_engine* gf_engine_create(void) { return new gf_engine(); }

void gf_engine_destroy(gf_engine* e) {
  if (!e) return;
  if (e->backend) e->backend->stop();
  delete e;
}

int gf_engine_start(gf_engine* e, double sample_rate, int buffer_frames, int input_channels,
                    int output_channels) {
  if (!e) return 0;
  e->sample_rate = sample_rate;
  e->buffer_frames = buffer_frames;
  e->output_channels = output_channels;
  e->capture_channels = input_channels > 0 ? input_channels : 1;
  // Prepare capture-capable (ring sized) but open the device playback-only — the
  // mic is only acquired during an actual take (gf_engine_start_take).
  e->engine.prepare(sample_rate, e->capture_channels, output_channels);
  return gfStartBackend(e, 0);
}

void gf_engine_stop(gf_engine* e) {
  if (e && e->backend) e->backend->stop();
}

int gf_engine_start_take(gf_engine* e, int count_in_bars) {
  if (!e) return 0;
  // Reopen as duplex (acquires the mic). On failure the backend keeps the reason
  // for gf_engine_last_error; restore playback-only so the app stays usable.
  if (!gfStartBackend(e, e->capture_channels)) {
    gfStartBackend(e, 0);
    return 0;
  }
  e->engine.startTake(count_in_bars);  // count-in, then play + capture from bar 0
  e->recording = true;
  return 1;
}

int gf_engine_stop_take(gf_engine* e) {
  if (!e || !e->recording) return 0;
  // Read reported latency while the duplex device is still open.
  const double round_trip =
      e->backend ? e->backend->outputLatencyFrames() + e->backend->inputLatencyFrames() : 0.0;
  e->engine.setRecording(false);
  e->engine.stop();                    // stop transport
  if (e->backend) e->backend->stop();  // audio thread gone -> safe to mutate tracks
  const bool committed = e->engine.commitTake(round_trip);
  gfStartBackend(e, 0);                // release the mic, back to playback-only
  e->recording = false;
  return committed ? 1 : 0;
}

int gf_engine_track_count(gf_engine* e) { return e ? e->engine.trackCount() : 0; }

void gf_engine_set_track_muted(gf_engine* e, int index, int muted) {
  if (e) e->engine.setTrackMuted(index, muted != 0);
}
void gf_engine_set_track_gain(gf_engine* e, int index, float gain) {
  if (e) e->engine.setTrackGain(index, gain);
}

int gf_engine_track_frame_count(gf_engine* e, int index) {
  return e ? e->engine.trackFrameCount(index) : 0;
}
int gf_engine_track_channels(gf_engine* e, int index) {
  return e ? e->engine.trackChannels(index) : 0;
}
long long gf_engine_track_start_frame(gf_engine* e, int index) {
  return e ? static_cast<long long>(e->engine.trackStartFrame(index)) : 0;
}
int gf_engine_copy_track_samples(gf_engine* e, int index, float* out, int max_samples) {
  return e ? e->engine.copyTrackSamples(index, out, max_samples) : 0;
}
int gf_engine_add_remote_track(gf_engine* e, const float* samples, int sample_count, int channels,
                               long long start_frame) {
  return e ? e->engine.addTrackData(samples, sample_count, channels,
                                    static_cast<int64_t>(start_frame))
           : -1;
}

void gf_engine_set_tempo(gf_engine* e, double bpm) {
  if (e) e->engine.setTempo(bpm);
}
void gf_engine_set_time_signature(gf_engine* e, int numerator, int denominator) {
  if (e) e->engine.setTimeSignature(numerator, denominator);
}
void gf_engine_set_metronome_enabled(gf_engine* e, int enabled) {
  if (e) e->engine.setMetronomeEnabled(enabled != 0);
}
void gf_engine_play(gf_engine* e) {
  if (e) e->engine.play();
}
void gf_engine_pause(gf_engine* e) {
  if (e) e->engine.stop();
}
void gf_engine_set_recording(gf_engine* e, int enabled) {
  if (e) e->engine.setRecording(enabled != 0);
}
long long gf_engine_play_head_frames(gf_engine* e) {
  return e ? static_cast<long long>(e->engine.playHeadFrames()) : 0;
}

const char* gf_engine_last_error(gf_engine* e) {
  if (e && e->backend) return e->backend->lastError();
  return "";
}

}  // extern "C"
