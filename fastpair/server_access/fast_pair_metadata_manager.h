// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_METADATA_MANAGER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_METADATA_MANAGER_H_

#include <functional>
#include <list>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "fastpair/proto/fastpair_rpcs.pb.h"
#include "internal/base/observer_list.h"

// The Fast pair metadata manager interfaces with the Nearby server in the
// the metadata downloaded

namespace nearby {
namespace fastpair {

enum class DeviceNameValidationResult {
  // The device name was valid.
  kValid = 0,
  // The device name must not be empty.
  kErrorEmpty = 1,
  // The device name is too long.
  kErrorTooLong = 2,
  // The device name is not valid UTF-8.
  kErrorNotValidUtf8 = 3
};

// The Fast Pair metadata manager interfaces
class FastPairMetadataManager {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnMetadataDownlownd(const std::optional<std::string>& model_id,
                                     const proto::Device& device) = 0;
  };
  FastPairMetadataManager();
  virtual ~FastPairMetadataManager();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts/Stops contact task scheduling.
  void Start();
  void Stop();
  bool is_running() { return is_running_; }

  virtual void DownloadMetadata() = 0;

 protected:
  virtual void OnStart() = 0;
  virtual void OnStop() = 0;

  void NotifyMetadataDownloaded(const std::optional<std::string>& model_id,
                                const proto::Device& device);

 private:
  bool is_running_ = false;
  ObserverList<Observer> observers_;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_METADATA_MANAGER_H_
