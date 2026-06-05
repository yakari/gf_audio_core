// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <cstdio>
#include <string>
#include <utility>

#include "gf_audio_core/audio_backend.h"

namespace gf {
namespace {

class MiniaudioBackend : public IAudioBackend {
 public:
  ~MiniaudioBackend() override { stop(); }

  bool start(const Config& config, RenderFn render) override {
    if (running_) return false;
    last_error_.clear();
    render_ = std::move(render);
    config_ = config;

    if (!initContext()) return false;

    const bool duplex = config.input_channels > 0;
    ma_device_config dc =
        ma_device_config_init(duplex ? ma_device_type_duplex : ma_device_type_playback);
    dc.sampleRate = static_cast<ma_uint32>(config.sample_rate);
    // Lock the period to our requested buffer size for predictable latency
    // (don't let the backend pick its own, e.g. 512), and ask for the
    // low-latency profile.
    dc.periodSizeInFrames = static_cast<ma_uint32>(config.buffer_frames);
    dc.performanceProfile = ma_performance_profile_low_latency;
    dc.playback.format = ma_format_f32;
    dc.playback.channels = static_cast<ma_uint32>(config.output_channels);
    if (duplex) {
      dc.capture.format = ma_format_f32;
      dc.capture.channels = static_cast<ma_uint32>(config.input_channels);
    }
    dc.dataCallback = &MiniaudioBackend::dataCallback;
    dc.pUserData = this;

    ma_result r = ma_device_init(&context_, &dc, &device_);
    if (r != MA_SUCCESS) {
      uninitContext();
      return fail("ma_device_init", r);
    }

    r = ma_device_start(&device_);
    if (r != MA_SUCCESS) {
      ma_device_uninit(&device_);
      uninitContext();
      return fail("ma_device_start", r);
    }

    running_ = true;
    std::fprintf(stderr,
                 "[gf_audio_core] audio started: backend=%s, %u Hz, period=%u frames, out=%d, "
                 "in=%d\n",
                 ma_get_backend_name(context_.backend), device_.sampleRate,
                 device_.playback.internalPeriodSizeInFrames, config.output_channels,
                 config.input_channels);
    std::fflush(stderr);
    return true;
  }

  void stop() override {
    if (running_) {
      ma_device_uninit(&device_);
      running_ = false;
    }
    uninitContext();
  }

  bool isRunning() const override { return running_; }
  double actualSampleRate() const override {
    return running_ ? device_.sampleRate : config_.sample_rate;
  }
  int actualBufferFrames() const override { return config_.buffer_frames; }

  // Estimate from the device's internal buffer depth (period size * period
  // count). This is only an approximation — a real Oboe/CoreAudio backend would
  // return the OS-reported figure. Good enough for desktop development.
  double outputLatencyFrames() const override {
    if (!running_) return 0.0;
    const double frames = static_cast<double>(device_.playback.internalPeriodSizeInFrames) *
                          device_.playback.internalPeriods;
    return frames > 0.0 ? frames : config_.buffer_frames * 2.0;
  }
  double inputLatencyFrames() const override {
    if (!running_ || config_.input_channels <= 0) return 0.0;
    const double frames = static_cast<double>(device_.capture.internalPeriodSizeInFrames) *
                          device_.capture.internalPeriods;
    return frames > 0.0 ? frames : config_.buffer_frames * 2.0;
  }

  const char* lastError() const override { return last_error_.c_str(); }

 private:
  // Initialize the miniaudio context. On desktop Linux we prefer the native
  // PulseAudio backend (which routes to PipeWire's pulse server) over raw ALSA,
  // because ALSA's pulse plugin is flaky for capture (the pcm_pulse
  // "Unable to create stream" failures). ALSA/JACK remain as fallbacks.
  bool initContext() {
    ma_result r;
#if defined(__linux__) && !defined(__ANDROID__)
    const ma_backend backends[] = {ma_backend_pulseaudio, ma_backend_alsa, ma_backend_jack};
    r = ma_context_init(backends, sizeof(backends) / sizeof(backends[0]), nullptr, &context_);
    if (r != MA_SUCCESS) {
      r = ma_context_init(nullptr, 0, nullptr, &context_);  // fall back to auto-detect
    }
#else
    r = ma_context_init(nullptr, 0, nullptr, &context_);
#endif
    if (r != MA_SUCCESS) return fail("ma_context_init", r);
    context_inited_ = true;
    return true;
  }

  void uninitContext() {
    if (context_inited_) {
      ma_context_uninit(&context_);
      context_inited_ = false;
    }
  }

  // Records a human-readable failure and logs it; always returns false so it can
  // be used as `return fail(...)`.
  bool fail(const char* stage, ma_result r) {
    last_error_ = std::string(stage) + ": " + ma_result_description(r);
    std::fprintf(stderr, "[gf_audio_core] audio init failed — %s\n", last_error_.c_str());
    std::fflush(stderr);
    return false;
  }

  // Realtime audio thread. Forwards straight to the engine render fn.
  static void dataCallback(ma_device* device, void* output, const void* input,
                           ma_uint32 frame_count) {
    auto* self = static_cast<MiniaudioBackend*>(device->pUserData);
    if (self->render_) {
      self->render_(static_cast<const float*>(input), static_cast<float*>(output),
                    static_cast<int>(frame_count));
    }
  }

  ma_context context_{};
  bool context_inited_ = false;
  ma_device device_{};
  RenderFn render_;
  Config config_{};
  bool running_ = false;
  std::string last_error_;
};

}  // namespace

std::unique_ptr<IAudioBackend> createMiniaudioBackend() {
  return std::make_unique<MiniaudioBackend>();
}

}  // namespace gf
