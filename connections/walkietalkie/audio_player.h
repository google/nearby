#ifndef WALKIETALKIE_AUDIO_PLAYER_H_
#define WALKIETALKIE_AUDIO_PLAYER_H_

#include <atomic>
#include <mutex>
#include <thread>

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#include "connections/payload.h"

class AudioPlayer {
 public:
  AudioPlayer(const AudioPlayer &) = delete;
  AudioPlayer(AudioPlayer &&) = delete;
  AudioPlayer &operator=(const AudioPlayer &) = delete;
  AudioPlayer &operator=(AudioPlayer &&) = delete;

  AudioPlayer()
      : stopped_(false),
        player_thread_(nullptr),
        pcm_(nullptr),
        pcm_params_(nullptr) {}
  ~AudioPlayer();

  void start(const std::shared_ptr<nearby::connections::Payload> &payload,
             int64_t size);
  void stop();

 private:
  void init_pcm();

  std::atomic_bool stopped_;
  std::unique_ptr<std::thread> player_thread_;

  snd_pcm_t *pcm_;
  snd_pcm_hw_params_t *pcm_params_;
};

#endif
