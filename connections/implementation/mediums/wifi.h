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

#ifndef CORE_INTERNAL_MEDIUMS_WIFI_H_
#define CORE_INTERNAL_MEDIUMS_WIFI_H_

// #include "internal/platform/mutex.h"
#include <string>

#include "internal/platform/mutex_lock.h"
#include "internal/platform/wifi.h"

namespace nearby {
namespace connections {

class Wifi {
 public:
  Wifi() = default;
  ~Wifi() = default;
  // Not copyable or movable
  Wifi(const Wifi&) = delete;
  Wifi& operator=(const Wifi&) = delete;
  Wifi(Wifi&&) = delete;
  Wifi& operator=(Wifi&&) = delete;

  // Returns true, if Wifi Medium are supported by a platform.
  bool IsAvailable() const ABSL_LOCKS_EXCLUDED(mutex_) {
    MutexLock lock(&mutex_);
    return IsAvailableLocked();
  }

  api::WifiCapability& GetCapability() {
    MutexLock lock(&mutex_);
    return medium_.GetCapability();
  }

  api::WifiInformation& GetInformation() {
    MutexLock lock(&mutex_);
    return medium_.GetInformation();
  }

  bool VerifyInternetConnectivity() {
    MutexLock lock(&mutex_);
    return medium_.VerifyInternetConnectivity();
  }

  std::string GetIpAddress() const {
    MutexLock lock(&mutex_);
    return medium_.GetIpAddress();
  }

 private:
  mutable Mutex mutex_;
  WifiMedium medium_ ABSL_GUARDED_BY(mutex_);

  // Same as IsAvailable(), but must be called with mutex_ held.
  bool IsAvailableLocked() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    if (medium_.IsValid()) return medium_.IsInterfaceValid();

    return false;
  }
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_WIFI_H_
