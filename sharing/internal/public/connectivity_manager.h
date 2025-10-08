// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONNECTIVITY_MANAGER_H_
#define THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONNECTIVITY_MANAGER_H_

#include <functional>

#include "absl/strings/string_view.h"

namespace nearby {

class ConnectivityManager {
 public:
  virtual ~ConnectivityManager() = default;

  virtual bool IsLanConnected() = 0;
  virtual bool IsInternetConnected() = 0;

  // Registers a listener for connection changes. The listener will be called
  // when the device is connected to a LAN network or when the device is
  // connected to internet.
  virtual void RegisterLanListener(absl::string_view listener_name,
                                   std::function<void(bool)>) = 0;
  virtual void UnregisterLanListener(absl::string_view listener_name) = 0;
  virtual void RegisterInternetListener(absl::string_view listener_name,
                                        std::function<void(bool)>) = 0;
  virtual void UnregisterInternetListener(absl::string_view listener_name) = 0;

  // Is the device a HP device with Realtek wireless module.
  virtual bool IsHPRealtekDevice() = 0;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_INTERNAL_PUBLIC_CONNECTIVITY_MANAGER_H_
