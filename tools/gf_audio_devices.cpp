// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

// Diagnostic: lists playback/capture devices for each audio backend and tries to
// open the default capture device. Use it to figure out why mic capture fails on
// a given Linux audio stack (PulseAudio/PipeWire vs ALSA vs JACK). Standalone —
// compiles its own miniaudio, links nothing from gf_audio_core.
//
//   ./gf_audio_devices
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <cstdio>

namespace {

// Opens (and briefly starts) a default capture device with the given format /
// channels; returns the result. ma_format_unknown + 0 channels = device native.
ma_result tryOpenCapture(ma_context* ctx, ma_format format, ma_uint32 channels) {
  ma_device_config dc = ma_device_config_init(ma_device_type_capture);
  dc.capture.format = format;
  dc.capture.channels = channels;
  dc.sampleRate = 48000;
  ma_device device;
  ma_result r = ma_device_init(ctx, &dc, &device);
  if (r != MA_SUCCESS) return r;
  const ma_result s = ma_device_start(&device);
  ma_device_uninit(&device);
  return s;  // MA_SUCCESS only if init AND start succeeded
}

void probe(const char* label, const ma_backend* backends, ma_uint32 count) {
  ma_context ctx;
  if (ma_context_init(backends, count, nullptr, &ctx) != MA_SUCCESS) {
    std::printf("[%s] context init FAILED (backend unavailable)\n\n", label);
    return;
  }
  std::printf("[%s] selected backend = %s\n", label, ma_get_backend_name(ctx.backend));

  ma_device_info* playback = nullptr;
  ma_uint32 playback_count = 0;
  ma_device_info* capture = nullptr;
  ma_uint32 capture_count = 0;
  if (ma_context_get_devices(&ctx, &playback, &playback_count, &capture, &capture_count) ==
      MA_SUCCESS) {
    std::printf("  capture devices (%u):\n", capture_count);
    for (ma_uint32 i = 0; i < capture_count; ++i)
      std::printf("    - %s%s\n", capture[i].name, capture[i].isDefault ? "  [default]" : "");
    std::printf("  playback devices (%u):\n", playback_count);
    for (ma_uint32 i = 0; i < playback_count; ++i)
      std::printf("    - %s%s\n", playback[i].name, playback[i].isDefault ? "  [default]" : "");
  } else {
    std::printf("  ma_context_get_devices FAILED\n");
  }

  const ma_result native = tryOpenCapture(&ctx, ma_format_unknown, 0);
  std::printf("  open default capture (native)   : %s\n",
              native == MA_SUCCESS ? "OK" : ma_result_description(native));
  const ma_result f32mono = tryOpenCapture(&ctx, ma_format_f32, 1);
  std::printf("  open default capture (f32 mono) : %s\n\n",
              f32mono == MA_SUCCESS ? "OK" : ma_result_description(f32mono));

  ma_context_uninit(&ctx);
}

}  // namespace

int main() {
  std::printf("=== gf_audio_core device probe ===\n\n");
  probe("auto", nullptr, 0);
  const ma_backend pulse[] = {ma_backend_pulseaudio};
  probe("pulseaudio", pulse, 1);
  const ma_backend alsa[] = {ma_backend_alsa};
  probe("alsa", alsa, 1);
  const ma_backend jack[] = {ma_backend_jack};
  probe("jack", jack, 1);
  return 0;
}
