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

#include <string>

#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/wifi.h"


#ifndef PLATFORM_PUBLIC_WIFI_H_
#define PLATFORM_PUBLIC_WIFI_H_

namespace nearby {

class WifiMedium {
 public:
  using Platform = api::ImplementationPlatform;

  WifiMedium() : impl_(Platform::CreateWifiMedium()) {}
  ~WifiMedium() = default;

  api::WifiCapability& GetCapability() {
    CHECK(impl_);
    return impl_->GetCapability();
  }

  api::WifiInformation& GetInformation() {
    CHECK(impl_);
    return impl_->GetInformation();
  }

  bool IsInterfaceValid() const {
    CHECK(impl_);
    return impl_->IsInterfaceValid();
  }

  bool VerifyInternetConnectivity() {
    CHECK(impl_);
    return impl_->VerifyInternetConnectivity();
  }

  std::string GetIpAddress() const {
    CHECK(impl_);
    return impl_->GetIpAddress();
  }

  bool IsValid() const { return impl_ != nullptr; }

  api::WifiMedium& GetImpl() {
    CHECK(impl_);
    return *impl_;
  }

 private:
  // Mutex mutex_;
  std::unique_ptr<api::WifiMedium> impl_;
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_WIFI_H_
