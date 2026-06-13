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

// Recording. gf_engine_start_take reopens the device as duplex (acquiring the
// mic), plays a count_in_bars-bar count-in (0 = none), then plays existing
// tracks and captures the performance from bar 0.
// gf_engine_stop_take finalizes: it latency-aligns the captured take onto the
// grid, adds it as a playable track, and returns the device to playback-only;
// returns 1 if a take was committed. gf_engine_track_count is the number of
// recorded tracks. On a start_take failure, gf_engine_last_error has the reason.
GF_API int gf_engine_start_take(gf_engine* e, int count_in_bars);
GF_API int gf_engine_stop_take(gf_engine* e);
GF_API int gf_engine_track_count(gf_engine* e);

// Per-take mix: mute/unmute and linear gain (1.0 = unity) for a recorded track
// by 0-based index (< track_count). RT-safe.
GF_API void gf_engine_set_track_muted(gf_engine* e, int index, int muted);
GF_API void gf_engine_set_track_gain(gf_engine* e, int index, float gain);

// Take export / import for LAN transfer. Samples are interleaved f32; sizes are
// in samples (frames * channels). Export a recorded track's PCM (copy into a
// caller-allocated buffer) plus its grid origin; import external PCM as a new
// track placed at start_frame (returns the new index, or -1 if full).
GF_API int gf_engine_track_frame_count(gf_engine* e, int index);
GF_API int gf_engine_track_channels(gf_engine* e, int index);
GF_API long long gf_engine_track_start_frame(gf_engine* e, int index);
GF_API int gf_engine_copy_track_samples(gf_engine* e, int index, float* out, int max_samples);
GF_API int gf_engine_add_remote_track(gf_engine* e, const float* samples, int sample_count,
                                      int channels, long long start_frame);

GF_API void gf_engine_set_tempo(gf_engine* e, double bpm);
GF_API void gf_engine_set_time_signature(gf_engine* e, int numerator, int denominator);
GF_API void gf_engine_set_metronome_enabled(gf_engine* e, int enabled);

GF_API void gf_engine_play(gf_engine* e);
GF_API void gf_engine_pause(gf_engine* e);
GF_API void gf_engine_set_recording(gf_engine* e, int enabled);

GF_API long long gf_engine_play_head_frames(gf_engine* e);

// Description of the last gf_engine_start failure, or "" if none / no engine.
// The returned pointer is owned by the engine and valid until the next start.
GF_API const char* gf_engine_last_error(gf_engine* e);

#ifdef __cplusplus
}
#endif

#endif  // GF_AUDIO_CORE_H
