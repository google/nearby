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

#ifndef PLATFORM_IMPL_ANDROID_WIFI_DIRECT_H_
#define PLATFORM_IMPL_ANDROID_WIFI_DIRECT_H_

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
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {
namespace android {

class WifiDirectMedium;

class WifiDirectSocket : public api::WifiDirectSocket {
 public:
  ~WifiDirectSocket() override = default;

  InputStream& GetInputStream() override;

  OutputStream& GetOutputStream() override;

  Exception Close() override;
};

class WifiDirectServerSocket : public api::WifiDirectServerSocket {
 public:
  ~WifiDirectServerSocket() override = default;

  std::string GetIPAddress() const override ABSL_LOCKS_EXCLUDED(mutex_);

  int GetPort() const override ABSL_LOCKS_EXCLUDED(mutex_);

  std::unique_ptr<api::WifiDirectSocket> Accept() override
      ABSL_LOCKS_EXCLUDED(mutex_);

  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  mutable absl::Mutex mutex_;
};

class WifiDirectMedium : public api::WifiDirectMedium {
 public:
  WifiDirectMedium();
  ~WifiDirectMedium() override;

  WifiDirectMedium(const WifiDirectMedium&) = delete;
  WifiDirectMedium(WifiDirectMedium&&) = delete;
  WifiDirectMedium& operator=(const WifiDirectMedium&) = delete;
  WifiDirectMedium& operator=(WifiDirectMedium&&) = delete;

  bool IsInterfaceValid() const override;

  std::unique_ptr<api::WifiDirectSocket> ConnectToService(
      absl::string_view ip_address, int port,
      CancellationFlag* cancellation_flag) override;

  // Advertiser starts to listen on server socket
  std::unique_ptr<api::WifiDirectServerSocket> ListenForService(
      int port) override;

  bool StartWifiDirect(WifiDirectCredentials* wifi_direct_credentials) override;
  bool StopWifiDirect() override;
  bool ConnectWifiDirect(
      WifiDirectCredentials* wifi_direct_credentials) override;
  bool DisconnectWifiDirect() override;

  std::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange()
      override;
};

}  // namespace android
}  // namespace nearby

#endif  // PLATFORM_IMPL_ANDROID_WIFI_DIRECT_H_
