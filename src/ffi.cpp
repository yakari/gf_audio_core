// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#include "gf_audio_core/gf_audio_core.h"

#include <memory>

#include "gf_audio_core/audio_backend.h"
#include "gf_audio_core/engine.h"

struct gf_engine {
  gf::Engine engine;
  std::unique_ptr<gf::IAudioBackend> backend;
};

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
  e->engine.prepare(sample_rate, input_channels, output_channels);
  e->backend = gf::createMiniaudioBackend();
  gf::IAudioBackend::Config cfg;
  cfg.sample_rate = sample_rate;
  cfg.buffer_frames = buffer_frames;
  cfg.input_channels = input_channels;
  cfg.output_channels = output_channels;
  gf::Engine* engine = &e->engine;
  return e->backend->start(cfg, [engine](const float* in, float* out, int n) {
    engine->process(in, out, n);
  })
             ? 1
             : 0;
}

void gf_engine_stop(gf_engine* e) {
  if (e && e->backend) e->backend->stop();
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
