// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Yann Poirier

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <atomic>
#include <utility>

#include "gf_audio_core/audio_backend.h"

namespace gf {
namespace {

class MiniaudioBackend : public IAudioBackend {
 public:
  ~MiniaudioBackend() override { stop(); }

  bool start(const Config& config, RenderFn render) override {
    if (running_) return false;
    render_ = std::move(render);
    config_ = config;

    const bool duplex = config.input_channels > 0;
    ma_device_config dc =
        ma_device_config_init(duplex ? ma_device_type_duplex : ma_device_type_playback);
    dc.sampleRate = static_cast<ma_uint32>(config.sample_rate);
    dc.periodSizeInFrames = static_cast<ma_uint32>(config.buffer_frames);
    dc.playback.format = ma_format_f32;
    dc.playback.channels = static_cast<ma_uint32>(config.output_channels);
    if (duplex) {
      dc.capture.format = ma_format_f32;
      dc.capture.channels = static_cast<ma_uint32>(config.input_channels);
    }
    dc.dataCallback = &MiniaudioBackend::dataCallback;
    dc.pUserData = this;

    if (ma_device_init(nullptr, &dc, &device_) != MA_SUCCESS) return false;
    if (ma_device_start(&device_) != MA_SUCCESS) {
      ma_device_uninit(&device_);
      return false;
    }
    running_ = true;
    return true;
  }

  void stop() override {
    if (running_) {
      ma_device_uninit(&device_);
      running_ = false;
    }
  }

  bool isRunning() const override { return running_; }
  double actualSampleRate() const override {
    return running_ ? device_.sampleRate : config_.sample_rate;
  }
  int actualBufferFrames() const override { return config_.buffer_frames; }

 private:
  // Realtime audio thread. Forwards straight to the engine render fn.
  static void dataCallback(ma_device* device, void* output, const void* input,
                           ma_uint32 frame_count) {
    auto* self = static_cast<MiniaudioBackend*>(device->pUserData);
    if (self->render_) {
      self->render_(static_cast<const float*>(input), static_cast<float*>(output),
                    static_cast<int>(frame_count));
    }
  }

  ma_device device_{};
  RenderFn render_;
  Config config_{};
  bool running_ = false;
};

}  // namespace

std::unique_ptr<IAudioBackend> createMiniaudioBackend() {
  return std::make_unique<MiniaudioBackend>();
}

}  // namespace gf
