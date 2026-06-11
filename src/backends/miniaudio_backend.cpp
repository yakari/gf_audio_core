// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "gf_audio_core/audio_backend.h"
#include "gf_audio_core/dsp/ring_buffer.h"

namespace gf {
namespace {

// Backend that presents a single duplex-style callback to the engine but, under
// the hood, runs the playback and capture as TWO SEPARATE miniaudio devices.
//
// A single ma_device_type_duplex device is unreliable on PulseAudio/PipeWire
// (ma_device_init "Unknown error"). Separate playback + capture devices work.
// The capture device pushes mic frames into an internal SPSC ring; the playback
// callback pops the matching frame count and hands them to the engine as `in`,
// so the engine keeps its simple in/out/num_frames contract.
//
// The two devices free-run on independent clocks, so the in/out pairing carries
// some skew (absorbed by latency compensation + the manual nudge). Fine for now.
class MiniaudioBackend : public IAudioBackend {
 public:
  ~MiniaudioBackend() override { stop(); }

  bool start(const Config& config, RenderFn render) override {
    if (running_) return false;
    last_error_.clear();
    render_ = std::move(render);
    config_ = config;
    const bool want_capture = config.input_channels > 0;

    if (!initContext()) return false;

    if (want_capture) {
      const size_t block = static_cast<size_t>(config.buffer_frames) * config.input_channels;
      capture_ring_.reset(block * 16);          // a few blocks of jitter headroom
      scratch_in_.assign(block, 0.0f);
    }

    if (!initPlaybackDevice()) {
      uninitContext();
      return false;
    }
    if (want_capture && !initCaptureDevice()) {
      ma_device_uninit(&playback_device_);
      uninitContext();
      return false;
    }

    // Start capture first so input is already flowing when playback pulls it.
    if (want_capture) {
      const ma_result r = ma_device_start(&capture_device_);
      if (r != MA_SUCCESS) {
        cleanupDevices();
        uninitContext();
        return fail("ma_device_start(capture)", r);
      }
    }
    const ma_result r = ma_device_start(&playback_device_);
    if (r != MA_SUCCESS) {
      cleanupDevices();
      uninitContext();
      return fail("ma_device_start(playback)", r);
    }

    running_ = true;
    std::fprintf(stderr,
                 "[gf_audio_core] audio started: backend=%s, %u Hz, period=%u frames, out=%d, "
                 "in=%d (separate devices)\n",
                 ma_get_backend_name(context_.backend), playback_device_.sampleRate,
                 playback_device_.playback.internalPeriodSizeInFrames, config.output_channels,
                 config.input_channels);
    std::fflush(stderr);
    return true;
  }

  void stop() override {
    if (running_) {
      cleanupDevices();
      running_ = false;
    }
    uninitContext();
  }

  bool isRunning() const override { return running_; }
  double actualSampleRate() const override {
    return running_ ? playback_device_.sampleRate : config_.sample_rate;
  }
  int actualBufferFrames() const override { return config_.buffer_frames; }

  double outputLatencyFrames() const override {
    if (!running_) return 0.0;
    const double frames = static_cast<double>(playback_device_.playback.internalPeriodSizeInFrames) *
                          playback_device_.playback.internalPeriods;
    return frames > 0.0 ? frames : config_.buffer_frames * 2.0;
  }
  double inputLatencyFrames() const override {
    if (!running_ || !capture_inited_) return 0.0;
    const double frames = static_cast<double>(capture_device_.capture.internalPeriodSizeInFrames) *
                          capture_device_.capture.internalPeriods;
    return frames > 0.0 ? frames : config_.buffer_frames * 2.0;
  }

  const char* lastError() const override { return last_error_.c_str(); }

 private:
  bool initContext() {
    ma_result r;
#if defined(__linux__) && !defined(__ANDROID__)
    const ma_backend backends[] = {ma_backend_pulseaudio, ma_backend_alsa, ma_backend_jack};
    r = ma_context_init(backends, sizeof(backends) / sizeof(backends[0]), nullptr, &context_);
    if (r != MA_SUCCESS) r = ma_context_init(nullptr, 0, nullptr, &context_);
#else
    r = ma_context_init(nullptr, 0, nullptr, &context_);
#endif
    if (r != MA_SUCCESS) return fail("ma_context_init", r);
    context_inited_ = true;
    return true;
  }

  bool initPlaybackDevice() {
    ma_device_config dc = ma_device_config_init(ma_device_type_playback);
    dc.sampleRate = static_cast<ma_uint32>(config_.sample_rate);
    dc.periodSizeInFrames = static_cast<ma_uint32>(config_.buffer_frames);
    dc.performanceProfile = ma_performance_profile_low_latency;
    dc.playback.format = ma_format_f32;
    dc.playback.channels = static_cast<ma_uint32>(config_.output_channels);
    dc.dataCallback = &MiniaudioBackend::playbackCallback;
    dc.pUserData = this;
    const ma_result r = ma_device_init(&context_, &dc, &playback_device_);
    if (r != MA_SUCCESS) return fail("ma_device_init(playback)", r);
    return true;
  }

  bool initCaptureDevice() {
    ma_device_config dc = ma_device_config_init(ma_device_type_capture);
    dc.sampleRate = static_cast<ma_uint32>(config_.sample_rate);
    dc.periodSizeInFrames = static_cast<ma_uint32>(config_.buffer_frames);
    dc.performanceProfile = ma_performance_profile_low_latency;
    dc.capture.format = ma_format_f32;
    dc.capture.channels = static_cast<ma_uint32>(config_.input_channels);
    dc.dataCallback = &MiniaudioBackend::captureCallback;
    dc.pUserData = this;
    const ma_result r = ma_device_init(&context_, &dc, &capture_device_);
    if (r != MA_SUCCESS) return fail("ma_device_init(capture)", r);
    capture_inited_ = true;
    return true;
  }

  void cleanupDevices() {
    ma_device_uninit(&playback_device_);
    if (capture_inited_) {
      ma_device_uninit(&capture_device_);
      capture_inited_ = false;
    }
  }

  void uninitContext() {
    if (context_inited_) {
      ma_context_uninit(&context_);
      context_inited_ = false;
    }
  }

  bool fail(const char* stage, ma_result r) {
    last_error_ = std::string(stage) + ": " + ma_result_description(r);
    std::fprintf(stderr, "[gf_audio_core] audio init failed — %s\n", last_error_.c_str());
    std::fflush(stderr);
    return false;
  }

  // Capture thread: push mic frames into the bridge ring.
  static void captureCallback(ma_device* device, void* /*output*/, const void* input,
                              ma_uint32 frame_count) {
    auto* self = static_cast<MiniaudioBackend*>(device->pUserData);
    self->capture_ring_.push(static_cast<const float*>(input),
                             static_cast<size_t>(frame_count) * self->config_.input_channels);
  }

  // Playback thread: pull the matching input frames (zero-filled on underrun)
  // and hand both directions to the engine.
  static void playbackCallback(ma_device* device, void* output, const void* /*input*/,
                               ma_uint32 frame_count) {
    auto* self = static_cast<MiniaudioBackend*>(device->pUserData);
    const float* in_ptr = nullptr;
    if (self->config_.input_channels > 0) {
      const size_t want = static_cast<size_t>(frame_count) * self->config_.input_channels;
      std::fill(self->scratch_in_.begin(), self->scratch_in_.end(), 0.0f);
      self->capture_ring_.pop(self->scratch_in_.data(), want);
      in_ptr = self->scratch_in_.data();
    }
    if (self->render_) self->render_(in_ptr, static_cast<float*>(output), static_cast<int>(frame_count));
  }

  ma_context context_{};
  bool context_inited_ = false;
  ma_device playback_device_{};
  ma_device capture_device_{};
  bool capture_inited_ = false;
  RenderFn render_;
  Config config_{};
  bool running_ = false;
  std::string last_error_;

  dsp::SpscRingBuffer<float> capture_ring_;  // capture thread -> playback thread
  std::vector<float> scratch_in_;            // playback-thread input scratch
};

}  // namespace

std::unique_ptr<IAudioBackend> createMiniaudioBackend() {
  return std::make_unique<MiniaudioBackend>();
}

}  // namespace gf
