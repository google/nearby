// Copyright 2020 Google LLC
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

#ifndef PLATFORM_PUBLIC_WIFI_HOTSPOT_H_
#define PLATFORM_PUBLIC_WIFI_HOTSPOT_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/implementation/platform.h"
#include "internal/platform/implementation/wifi_hotspot.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/wifi_credential.h"

namespace nearby {

// Socket class for both SoftAP ( created through accept connection from STA)
// and STA side (created through connect request to SoftAP side)
// Class WifiHotspotSocket is copyable & movable
class WifiHotspotSocket final {
 public:
  WifiHotspotSocket() = default;
  WifiHotspotSocket(const WifiHotspotSocket&) = default;
  WifiHotspotSocket& operator=(const WifiHotspotSocket&) = default;
  WifiHotspotSocket(WifiHotspotSocket&&) = default;
  WifiHotspotSocket& operator=(WifiHotspotSocket&&) = default;

  explicit WifiHotspotSocket(std::unique_ptr<api::WifiHotspotSocket> socket)
      : impl_(std::move(socket)) {}

  // Returns the InputStream of the WifiHotspotSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiHotspotSocket object is destroyed.
  InputStream& GetInputStream() {
    CHECK(impl_);
    return impl_->GetInputStream();
  }

  // Returns the OutputStream of the WifiHotspotSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiHotspotSocket object is destroyed.
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
  // an object returned by WifiHotspotMedium::Connect
  // These methods may also return an invalid socket if connection failed for
  // any reason.
  bool IsValid() const { return impl_ != nullptr; }

  // Returns reference to platform implementation.
  // This is used to communicate with platform code, and for debugging purposes.
  // Returned reference will remain valid for while WifiHotspotSocket object is
  // itself valid. Typically WifiHotspotSocket lifetime matches duration of the
  // connection, and is controlled by end user, since they hold the instance.
  api::WifiHotspotSocket& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<api::WifiHotspotSocket> impl_;
};

// Server socket class for SoftAP when it listens to connection request.
class WifiHotspotServerSocket final {
 public:
  WifiHotspotServerSocket() = default;
  WifiHotspotServerSocket(const WifiHotspotServerSocket&) = default;
  WifiHotspotServerSocket& operator=(const WifiHotspotServerSocket&) = default;

  explicit WifiHotspotServerSocket(
      std::unique_ptr<api::WifiHotspotServerSocket> socket)
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
  WifiHotspotSocket Accept() {
    std::unique_ptr<api::WifiHotspotSocket> socket = impl_->Accept();
    if (!socket) {
      NEARBY_LOGS(INFO)
          << "WifiHotspotServerSocket Accept() failed on server socket: ";
    }
    return WifiHotspotSocket(std::move(socket));
  }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() {
    NEARBY_LOGS(INFO) << "WifiHotspotServerSocket Closing:: " << this;
    return impl_->Close();
  }

  bool IsValid() const { return impl_ != nullptr; }
  api::WifiHotspotServerSocket& GetImpl() {
    CHECK(impl_);
    return *impl_;
  }

 private:
  std::shared_ptr<api::WifiHotspotServerSocket> impl_;
};

// Container of operations that can be performed over the WifiHotspot medium.
class WifiHotspotMedium {
 public:
  using Platform = api::ImplementationPlatform;

  WifiHotspotMedium() : impl_(Platform::CreateWifiHotspotMedium()) {}

  // Returns a new WifiHotspotSocket by ip address and port.
  // On Success, WifiHotspotSocket::IsValid()returns true.
  WifiHotspotSocket ConnectToService(absl::string_view ip_address, int port,
                                     CancellationFlag* cancellation_flag);

  // Returns a new WifiHotspotServerSocket.
  // On Success, WifiHotspotServerSocket::IsValid() returns true.
  WifiHotspotServerSocket ListenForService(int port = 0) {
    return WifiHotspotServerSocket(impl_->ListenForService(port));
  }

  // Returns the port range as a pair of min and max port.
  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange() {
    return impl_->GetDynamicPortRange();
  }

  bool StartWifiHotspot() {
    MutexLock lock(&mutex_);
    return impl_->StartWifiHotspot(&hotspot_credentials_);
  }
  bool StopWifiHotspot() { return impl_->StopWifiHotspot(); }

  bool ConnectWifiHotspot(const std::string& ssid,
                          const std::string& password) {
    MutexLock lock(&mutex_);
    hotspot_credentials_.SetSSID(ssid);
    hotspot_credentials_.SetPassword(password);
    return impl_->ConnectWifiHotspot(&hotspot_credentials_);
  }
  bool DisconnectWifiHotspot() { return impl_->DisconnectWifiHotspot(); }

  HotspotCredentials* GetCredential() {
    return &hotspot_credentials_;
  }

  bool IsInterfaceValid() const {
    CHECK(impl_);
    return impl_->IsInterfaceValid();
  }

  bool IsValid() const { return impl_ != nullptr; }

  // since impl_ could be nullptr if platform hasn't implemented this Medium,
  // call IsValid() to check the availability before call this method
  api::WifiHotspotMedium& GetImpl() {
    CHECK(impl_);
    return *impl_;
  }

 private:
  Mutex mutex_;
  std::unique_ptr<api::WifiHotspotMedium> impl_;
  HotspotCredentials hotspot_credentials_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_WIFI_HOTSPOT_H_
