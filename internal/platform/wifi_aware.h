// Copyright 2026 Google LLC
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

#ifndef PLATFORM_PUBLIC_WIFI_AWARE_H_
#define PLATFORM_PUBLIC_WIFI_AWARE_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/upgrade_address_info.h"
#include "internal/platform/implementation/wifi_aware.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/listeners.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/socket.h"
#include "internal/platform/wifi_aware_service_info.h"

namespace nearby {

class WifiAwareSocket final : public MediumSocket {
 public:
  WifiAwareSocket()
      : MediumSocket(location::nearby::proto::connections::Medium::WIFI_AWARE) {
  }
  WifiAwareSocket(const WifiAwareSocket&) = default;
  WifiAwareSocket& operator=(const WifiAwareSocket&) = default;
  ~WifiAwareSocket() override = default;

  // Creates a physical WifiAwareSocket from a platform implementation.
  explicit WifiAwareSocket(std::unique_ptr<api::WifiAwareSocket> socket)
      : MediumSocket(location::nearby::proto::connections::Medium::WIFI_AWARE),
        impl_(std::move(socket)) {}

  InputStream& GetInputStream() override { return impl_->GetInputStream(); }
  OutputStream& GetOutputStream() override { return impl_->GetOutputStream(); }

  Exception Close() override {
    if (!IsValid()) {
      return {Exception::kSuccess};
    }
    return impl_->Close();
  }

  bool IsValid() const { return impl_ != nullptr; }

  std::shared_ptr<api::WifiAwareSocket> GetImpl() const { return impl_; }

 private:
  std::shared_ptr<api::WifiAwareSocket> impl_;
};

class WifiAwareServerSocket final {
 public:
  WifiAwareServerSocket() = default;
  WifiAwareServerSocket(const WifiAwareServerSocket&) = default;
  WifiAwareServerSocket& operator=(const WifiAwareServerSocket&) = default;
  ~WifiAwareServerSocket() = default;
  explicit WifiAwareServerSocket(
      std::unique_ptr<api::WifiAwareServerSocket> socket)
      : impl_(std::move(socket)) {}

  WifiAwareSocket Accept() {
    if (!IsValid()) {
      return WifiAwareSocket();
    }
    std::unique_ptr<api::WifiAwareSocket> socket = impl_->Accept();
    if (!socket) {
      LOG(INFO) << "WifiAwareServerSocket Accept() failed";
    }
    return WifiAwareSocket(std::move(socket));
  }

  Exception Close() {
    if (!IsValid()) {
      return {Exception::kSuccess};
    }
    return impl_->Close();
  }

  bool IsValid() const { return impl_ != nullptr; }
  std::shared_ptr<api::WifiAwareServerSocket> GetImpl() const { return impl_; }

 private:
  std::shared_ptr<api::WifiAwareServerSocket> impl_;
};

class WifiAwareMedium final {
 public:
  struct DiscoveredServiceCallback {
    absl::AnyInvocable<void(WifiAwareServiceInfo wifi_aware_service_info,
                            absl::string_view service_type)>
        service_discovered_cb =
            DefaultCallback<WifiAwareServiceInfo, absl::string_view>();
    absl::AnyInvocable<void(WifiAwareServiceInfo wifi_aware_service_info,
                            absl::string_view service_type)>
        service_lost_cb =
            DefaultCallback<WifiAwareServiceInfo, absl::string_view>();
  };

  WifiAwareMedium()
      : impl_(api::ImplementationPlatform::CreateWifiAwareMedium()) {}
  explicit WifiAwareMedium(std::unique_ptr<api::WifiAwareMedium> impl)
      : impl_(std::move(impl)) {}
  ~WifiAwareMedium() = default;

  bool StartAdvertising(const WifiAwareServiceInfo& wifi_aware_service_info);
  bool StopAdvertising(const WifiAwareServiceInfo& wifi_aware_service_info);

  bool StartDiscovery(absl::string_view service_type,
                      DiscoveredServiceCallback callback);
  bool StopDiscovery(absl::string_view service_type);

  bool IsPublishing() { return IsValid() && impl_->IsPublishing(); }
  bool StartPublishing() { return IsValid() && impl_->StartPublishing(); }
  bool StopPublishing() { return IsValid() && impl_->StopPublishing(); }

  bool IsSubscribing() { return IsValid() && impl_->IsSubscribing(); }
  bool StartSubscribing() { return IsValid() && impl_->StartSubscribing(); }
  bool StopSubscribing() { return IsValid() && impl_->StopSubscribing(); }

  WifiAwareSocket ConnectToService(
      const WifiAwareServiceInfo& remote_service_info,
      CancellationFlag* cancellation_flag);

  WifiAwareServerSocket ListenForService(int port = 0);

  bool IsValid() const { return impl_ != nullptr; }

  api::WifiAwareMedium* GetImpl() const {
    if (!IsValid()) {
      return nullptr;
    }
    return impl_.get();
  }

  api::UpgradeAddressInfo GetUpgradeAddressCandidates(
      const WifiAwareServerSocket& server_socket);

 private:
  struct DiscoveryCallbackInfo {
    std::shared_ptr<DiscoveredServiceCallback> medium_callback;
    // Discovered service names for this service type, used to deduplicate
    // service discovered callbacks and track active services for service lost
    // callbacks.
    absl::flat_hash_set<std::string> services;
  };

  mutable Mutex mutex_;
  std::unique_ptr<api::WifiAwareMedium> impl_;
  // A map from service type to its discovery callback and discovered services.
  absl::flat_hash_map<std::string, DiscoveryCallbackInfo>
      service_type_to_callback_info_map_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_WIFI_AWARE_H_
