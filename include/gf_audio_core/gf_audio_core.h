// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

// C ABI for the Flutter (dart:ffi) binding. Keep this header C-clean and stable;
// it is the only surface the Dart side links against.
#ifndef GF_AUDIO_CORE_H
#define GF_AUDIO_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#define GF_API __declspec(dllexport)
#else
#define GF_API __attribute__((visibility("default")))
#endif

typedef struct gf_engine gf_engine;

GF_API gf_engine* gf_engine_create(void);
GF_API void gf_engine_destroy(gf_engine* e);

// Allocates the engine and opens the audio device. Returns 1 on success, 0 on
// failure.
GF_API int gf_engine_start(gf_engine* e, double sample_rate, int buffer_frames,
                           int input_channels, int output_channels);
GF_API void gf_engine_stop(gf_engine* e);

GF_API void gf_engine_set_tempo(gf_engine* e, double bpm);
GF_API void gf_engine_set_time_signature(gf_engine* e, int numerator, int denominator);
GF_API void gf_engine_set_metronome_enabled(gf_engine* e, int enabled);

GF_API void gf_engine_play(gf_engine* e);
GF_API void gf_engine_pause(gf_engine* e);
GF_API void gf_engine_set_recording(gf_engine* e, int enabled);

GF_API long long gf_engine_play_head_frames(gf_engine* e);

#ifdef __cplusplus
}
#endif

#endif  // GF_AUDIO_CORE_H
