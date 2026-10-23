// Copyright 2024 Google LLC
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

#ifndef PLATFORM_IMPL_ANDROID_WIFI_HOTSPOT_H_
#define PLATFORM_IMPL_ANDROID_WIFI_HOTSPOT_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {
namespace android {

class WifiHotspotMedium;

class WifiHotspotSocket : public api::WifiHotspotSocket {
 public:
  ~WifiHotspotSocket() override = default;

  InputStream& GetInputStream() override;

  OutputStream& GetOutputStream() override;

  Exception Close() override;
};

class WifiHotspotServerSocket : public api::WifiHotspotServerSocket {
 public:
  ~WifiHotspotServerSocket() override;

  std::string GetIPAddress() const override ABSL_LOCKS_EXCLUDED(mutex_);

  int GetPort() const override ABSL_LOCKS_EXCLUDED(mutex_);

  std::unique_ptr<api::WifiHotspotSocket> Accept() override
      ABSL_LOCKS_EXCLUDED(mutex_);

  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  mutable absl::Mutex mutex_;
};

// Container of operations that can be performed over the WifiHotspot medium.
class WifiHotspotMedium : public api::WifiHotspotMedium {
 public:
  WifiHotspotMedium();
  ~WifiHotspotMedium() override;

  WifiHotspotMedium(const WifiHotspotMedium&) = delete;
  WifiHotspotMedium(WifiHotspotMedium&&) = delete;
  WifiHotspotMedium& operator=(const WifiHotspotMedium&) = delete;
  WifiHotspotMedium& operator=(WifiHotspotMedium&&) = delete;

  bool IsInterfaceValid() const override;

  std::unique_ptr<api::WifiHotspotSocket> ConnectToService(
      absl::string_view ip_address, int port,
      CancellationFlag* cancellation_flag) override;

  std::unique_ptr<api::WifiHotspotServerSocket> ListenForService(
      int port) override;

  bool StartWifiHotspot(HotspotCredentials* hotspot_credentials) override;
  bool StopWifiHotspot() override;
  bool ConnectWifiHotspot(HotspotCredentials* hotspot_credentials) override;
  bool DisconnectWifiHotspot() override;

  std::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override;
};

}  // namespace android
}  // namespace nearby

#endif  // PLATFORM_IMPL_ANDROID_WIFI_HOTSPOT_H_
