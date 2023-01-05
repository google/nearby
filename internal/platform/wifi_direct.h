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

#ifndef PLATFORM_PUBLIC_WIFI_DIRECT_H_
#define PLATFORM_PUBLIC_WIFI_DIRECT_H_

#include <memory>
#include <string>
#include <utility>

#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/wifi_direct.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {

// Socket class for both WifiDirect GO ( created through accept connection from
// STA) and STA side (created through connect request to WifiDirect GO side)
// Class WifiDirectSocket is copyable & movable
class WifiDirectSocket final {
 public:
  WifiDirectSocket() = default;
  WifiDirectSocket(const WifiDirectSocket&) = default;
  WifiDirectSocket& operator=(const WifiDirectSocket&) = default;
  WifiDirectSocket(WifiDirectSocket&&) = default;
  WifiDirectSocket& operator=(WifiDirectSocket&&) = default;

  explicit WifiDirectSocket(std::unique_ptr<api::WifiDirectSocket> socket)
      : impl_(std::move(socket)) {}

  // Returns the InputStream of the WifiDirectSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiDirectSocket object is destroyed.
  InputStream& GetInputStream() {
    CHECK(impl_);
    return impl_->GetInputStream();
  }

  // Returns the OutputStream of the WifiDirectSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiDirectSocket object is destroyed.
  OutputStream& GetOutputStream() {
    CHECK(impl_);
    return impl_->GetOutputStream();
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() {
    CHECK(impl_);
    return impl_->Close();
  }

  // Returns true if a socket is usable. If this method returns false,
  // it is not safe to call any other method.
  // NOTE(socket validity):
  // Socket created by a default public constructor is not valid, because
  // it is missing platform implementation.
  // The only way to obtain a valid socket is through connection, such as
  // an object returned by WifiDirectMedium::Connect
  // These methods may also return an invalid socket if connection failed for
  // any reason.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  // Returned reference will remain valid for while WifiDirectSocket object is
  // itself valid. Typically WifiDirectSocket lifetime matches duration of the
  // connection, and is controlled by end user, since they hold the instance.
  api::WifiDirectSocket& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::WifiDirectSocket> impl_;
};

// Server socket class for WifiDirect GO when it listens to connection request.
class WifiDirectServerSocket final {
 public:
  WifiDirectServerSocket() = default;
  WifiDirectServerSocket(const WifiDirectServerSocket&) = default;
  WifiDirectServerSocket& operator=(const WifiDirectServerSocket&) = default;

  explicit WifiDirectServerSocket(
      std::unique_ptr<api::WifiDirectServerSocket> socket)
      : impl_(std::move(socket)) {}

  // Returns ip address.
  std::string GetIPAddress() const {
    CHECK(impl_);
    return impl_->GetIPAddress();
  }

  // Returns port.
  int GetPort() const {
    CHECK(impl_);
    return impl_->GetPort();
  }

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // On error, "impl_" will be nullptr and the caller will check it by calling
  // member function "IsValid()"
  // Once error is reported, it is permanent, and
  // ServerSocket has to be closed by caller.
  WifiDirectSocket Accept() {
    std::unique_ptr<api::WifiDirectSocket> socket = impl_->Accept();
    if (!socket) {
      NEARBY_LOGS(INFO)
          << "WifiDirectServerSocket Accept() failed on server socket: ";
    }
    return WifiDirectSocket(std::move(socket));
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() {
    NEARBY_LOGS(INFO) << "WifiDirectServerSocket Closing:: " << this;
    return impl_->Close();
  }

  bool IsValid() const { return impl_ != nullptr; }
  api::WifiDirectServerSocket& GetImpl() {
    CHECK(impl_);
    return *impl_;
  }

 private:
  std::shared_ptr<api::WifiDirectServerSocket> impl_;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiDirectMedium {
 public:
  using Platform = api::ImplementationPlatform;

  WifiDirectMedium() : impl_(Platform::CreateWifiDirectMedium()) {}
  ~WifiDirectMedium() = default;

  // Returns a new WifiDirectSocket by ip address and port.
  // On Success, WifiDirectSocket::IsValid() returns true.
  WifiDirectSocket ConnectToService(absl::string_view ip_address, int port,
                                    CancellationFlag* cancellation_flag);

  // Returns a new WifiDirectServerSocket.
  // On Success, WifiDirectServerSocket::IsValid() returns true.
  WifiDirectServerSocket ListenForService(int port = 0) {
    return WifiDirectServerSocket(impl_->ListenForService(port));
  }

  // Returns the port range as a pair of min and max port.
  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange() {
    return impl_->GetDynamicPortRange();
  }

  bool StartWifiDirect() {
    MutexLock lock(&mutex_);
    return impl_->StartWifiDirect(&wifi_direct_credentials_);
  }
  bool StopWifiDirect() { return impl_->StopWifiDirect(); }

  bool ConnectWifiDirect(absl::string_view ssid, absl::string_view password) {
    MutexLock lock(&mutex_);
    wifi_direct_credentials_.SetSSID(std::string(ssid));
    wifi_direct_credentials_.SetPassword(std::string(password));
    return impl_->ConnectWifiDirect(&wifi_direct_credentials_);
  }
  bool DisconnectWifiDirect() { return impl_->DisconnectWifiDirect(); }

  WifiDirectCredentials* GetCredential() { return &wifi_direct_credentials_; }

  bool IsInterfaceValid() const {
    CHECK(impl_);
    return impl_->IsInterfaceValid();
  }

  bool IsValid() const { return impl_ != nullptr; }
  api::WifiDirectMedium& GetImpl() {
    CHECK(impl_);
    return *impl_;
  }

 private:
  Mutex mutex_;
  std::unique_ptr<api::WifiDirectMedium> impl_;
  WifiDirectCredentials wifi_direct_credentials_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_WIFI_DIRECT_H_
