// Copyright 2025 Google LLC
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

#ifndef PLATFORM_IMPL_APPLE_WIFI_H_
#define PLATFORM_IMPL_APPLE_WIFI_H_

#import <Foundation/Foundation.h> // NOLINT
#import <Network/Network.h>

#include <string>
#include <utility>

#include "internal/platform/implementation/wifi.h"
#include "absl/strings/string_view.h"

namespace nearby {
namespace apple {

/**
 * Concrete WifiMedium implementation.
 */
class WifiMedium : public api::WifiMedium {
 public:
  WifiMedium() = default;
  ~WifiMedium() override = default;

  WifiMedium(const WifiMedium&) = delete;
  WifiMedium& operator=(const WifiMedium&) = delete;

  /**
   * @brief Returns true if the Wifi interface is valid.
   */
  bool IsInterfaceValid() const override { return true; }

  /**
   * @brief Returns the WifiCapability of the current Wifi interface.
   */
  api::WifiCapability& GetCapability() override {
    capability_.supports_5_ghz = true;
    return capability_;
  }

  /**
   * @brief Returns the WifiInformation of the current Wifi interface.
   */
  api::WifiInformation& GetInformation() override { return information_; }

 private:
  api::WifiCapability capability_ = {};
  api::WifiInformation information_ = {};
};




}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_WIFI_H_
