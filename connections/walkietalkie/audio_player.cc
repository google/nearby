#include <array>
#include <cassert>
#include <iostream>
#include <mutex>

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#include "connections/payload.h"
#include "connections/walkietalkie/audio_player.h"

AudioPlayer::~AudioPlayer() {
  stop();
  snd_pcm_drain(pcm_);
  snd_pcm_close(pcm_);
}

void AudioPlayer::start(
    const std::shared_ptr<nearby::connections::Payload> &payload,
    int64_t size) {
  stop();
  init_pcm();
  stopped_ = false;

  player_thread_ = std::make_unique<std::thread>([&, payload, size]() {
    auto *stream = payload->AsStream();
    assert(stream != nullptr);

    auto result = stream->ReadExactly(size);
    if (!result.ok()) {
      LOG(INFO) << "error reading from payload, exception value: "
                << result.GetException().value;
      return;
    }

    auto buffer = result.GetResult();
    auto ret = snd_pcm_writei(pcm_, buffer.data(),
                              snd_pcm_bytes_to_frames(pcm_, buffer.size()));
    if (ret == -EPIPE) {
      if (ret = snd_pcm_prepare(pcm_); ret < 0) {
        LOG(INFO) << "error preparing PCM device: " << snd_strerror(ret);
      }
    } else if (ret < 0) {
      LOG(INFO) << "error writing to PCM device: " << snd_strerror(ret);
      return;
    }
  });
}

void AudioPlayer::stop() {
  stopped_ = true;
  if (player_thread_ != nullptr && player_thread_->joinable())
    player_thread_->join();
}

void AudioPlayer::init_pcm() {
  if (pcm_ != nullptr) return;

  auto ret = snd_pcm_open(&pcm_, "default", SND_PCM_STREAM_PLAYBACK, 0);
  if (ret < 0) {
    LOG(ERROR) << "error opening PCM device: " << snd_strerror(ret);
    return;
  }

  snd_pcm_hw_params_alloca(&pcm_params_);
  snd_pcm_hw_params_any(pcm_, pcm_params_);
  if (ret = snd_pcm_hw_params_set_access(pcm_, pcm_params_,
                                         SND_PCM_ACCESS_RW_INTERLEAVED);
      ret < 0) {
    LOG(ERROR) << "error setting access type: " << snd_strerror(ret);
    return;
  }

  if (ret = snd_pcm_hw_params_set_format(pcm_, pcm_params_,
                                         SND_PCM_FORMAT_S16_LE);
      ret < 0) {
    LOG(ERROR) << "error setting PCM format: " << snd_strerror(ret);
    return;
  }

  if (ret = snd_pcm_hw_params_set_channels(pcm_, pcm_params_, 1); ret < 0) {
    LOG(ERROR) << "error setting number of channels: " << snd_strerror(ret);
    return;
  }

  const std::array<unsigned int, 6> possible_sample_rates = {
      8000, 11025, 16000, 22050, 44100, 48000};
  for (auto sample_rate : possible_sample_rates) {
    if (snd_pcm_hw_params_test_rate(pcm_, pcm_params_, sample_rate, 0) == 0) {
      if (ret = snd_pcm_hw_params_set_rate_near(pcm_, pcm_params_, &sample_rate,
                                                nullptr);
          ret < 0) {
        LOG(ERROR) << "error setting sample rate to " << sample_rate << ": "
                   << snd_strerror(ret);
        return;
      }
      break;
    }
  }

  if (ret = snd_pcm_hw_params(pcm_, pcm_params_); ret < 0) {
    LOG(ERROR) << "error setting hardware params: " << snd_strerror(ret);
    return;
  }
}
