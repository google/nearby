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

#include "platform/impl/g3/wifi_lan_v2.h"

#include <iostream>
#include <memory>
#include <string>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "platform/api/wifi_lan_v2.h"
#include "platform/base/cancellation_flag_listener.h"
#include "platform/base/logging.h"
#include "platform/base/medium_environment.h"
#include "platform/base/nsd_service_info.h"

namespace location {
namespace nearby {
namespace g3 {

WifiLanSocketV2::~WifiLanSocketV2() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

void WifiLanSocketV2::Connect(WifiLanSocketV2& other) {
  absl::MutexLock lock(&mutex_);
  remote_socket_ = &other;
  input_ = other.output_;
}

InputStream& WifiLanSocketV2::GetInputStream() {
  auto* remote_socket = GetRemoteSocket();
  CHECK(remote_socket != nullptr);
  return remote_socket->GetLocalInputStream();
}

OutputStream& WifiLanSocketV2::GetOutputStream() {
  return GetLocalOutputStream();
}

WifiLanSocketV2* WifiLanSocketV2::GetRemoteSocket() {
  absl::MutexLock lock(&mutex_);
  return remote_socket_;
}

bool WifiLanSocketV2::IsConnected() const {
  absl::MutexLock lock(&mutex_);
  return IsConnectedLocked();
}

bool WifiLanSocketV2::IsClosed() const {
  absl::MutexLock lock(&mutex_);
  return closed_;
}

Exception WifiLanSocketV2::Close() {
  absl::MutexLock lock(&mutex_);
  DoClose();
  return {Exception::kSuccess};
}

void WifiLanSocketV2::DoClose() {
  if (!closed_) {
    remote_socket_ = nullptr;
    output_->GetOutputStream().Close();
    output_->GetInputStream().Close();
    if (IsConnectedLocked()) {
      input_->GetOutputStream().Close();
      input_->GetInputStream().Close();
    }
    closed_ = true;
  }
}

bool WifiLanSocketV2::IsConnectedLocked() const { return input_ != nullptr; }

InputStream& WifiLanSocketV2::GetLocalInputStream() {
  absl::MutexLock lock(&mutex_);
  return output_->GetInputStream();
}

OutputStream& WifiLanSocketV2::GetLocalOutputStream() {
  absl::MutexLock lock(&mutex_);
  return output_->GetOutputStream();
}

std::string WifiLanServerSocketV2::GetIpAddress() {
  absl::MutexLock lock(&mutex_);
  return {};
}

int WifiLanServerSocketV2::GetPort() {
  absl::MutexLock lock(&mutex_);
  return 0;
}

std::unique_ptr<api::WifiLanSocketV2> WifiLanServerSocketV2::Accept() {
  absl::MutexLock lock(&mutex_);
  return nullptr;
}

bool WifiLanServerSocketV2::Connect(WifiLanSocketV2& socket) {
  absl::MutexLock lock(&mutex_);
  return true;
}

void WifiLanServerSocketV2::SetCloseNotifier(std::function<void()> notifier) {
  absl::MutexLock lock(&mutex_);
}

WifiLanServerSocketV2::~WifiLanServerSocketV2() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

Exception WifiLanServerSocketV2::Close() {
  absl::MutexLock lock(&mutex_);
  return DoClose();
}

Exception WifiLanServerSocketV2::DoClose() { return {Exception::kSuccess}; }

WifiLanMediumV2::WifiLanMediumV2() {
  auto& env = MediumEnvironment::Instance();
  env.RegisterWifiLanMediumV2(*this);
}

WifiLanMediumV2::~WifiLanMediumV2() {
  auto& env = MediumEnvironment::Instance();
  env.UnregisterWifiLanMediumV2(*this);
}

bool WifiLanMediumV2::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
  std::string service_type = nsd_service_info.GetServiceType();
  NEARBY_LOGS(INFO) << "G3 WifiLan StartAdvertising: nsd_service_info="
                    << &nsd_service_info
                    << ", service_name=" << nsd_service_info.GetServiceName()
                    << ", service_type=" << service_type;
  {
    absl::MutexLock lock(&mutex_);
    if (advertising_info_.Existed(service_type)) {
      NEARBY_LOGS(INFO)
          << "G3 WifiLan StartAdvertising: Can't start advertising because "
             "service_type="
          << service_type << ", has started already.";
      return false;
    }
  }
  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumV2ForAdvertising(*this, nsd_service_info,
                                          /*enabled=*/true);
  {
    absl::MutexLock lock(&mutex_);
    advertising_info_.Add(service_type);
  }
  return true;
}

bool WifiLanMediumV2::StopAdvertising(const NsdServiceInfo& nsd_service_info) {
  std::string service_type = nsd_service_info.GetServiceType();
  NEARBY_LOGS(INFO) << "G3 WifiLan StopAdvertising: nsd_service_info="
                    << &nsd_service_info
                    << ", service_name=" << nsd_service_info.GetServiceName()
                    << ", service_type=" << service_type;
  {
    absl::MutexLock lock(&mutex_);
    if (!advertising_info_.Existed(service_type)) {
      NEARBY_LOGS(INFO)
          << "G3 WifiLan StopAdvertising: Can't stop advertising because "
             "we never started advertising for service_type="
          << service_type;
      return false;
    }
    advertising_info_.Remove(service_type);
  }
  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumV2ForAdvertising(*this, nsd_service_info,
                                          /*enabled=*/false);
  return true;
}

bool WifiLanMediumV2::StartDiscovery(const std::string& service_type,
                                     DiscoveredServiceCallback callback) {
  NEARBY_LOGS(INFO) << "G3 WifiLan StartDiscovery: service_type="
                    << service_type;
  {
    absl::MutexLock lock(&mutex_);
    if (discovering_info_.Existed(service_type)) {
      NEARBY_LOGS(INFO)
          << "G3 WifiLan StartDiscovery: Can't start discovery because "
             "service_type="
          << service_type << " has started already.";
      return false;
    }
  }
  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumV2ForDiscovery(*this, std::move(callback),
                                        service_type, true);
  {
    absl::MutexLock lock(&mutex_);
    discovering_info_.Add(service_type);
  }
  return true;
}

bool WifiLanMediumV2::StopDiscovery(const std::string& service_type) {
  NEARBY_LOGS(INFO) << "G3 WifiLan StopDiscovery: service_type="
                    << service_type;
  {
    absl::MutexLock lock(&mutex_);
    if (!discovering_info_.Existed(service_type)) {
      NEARBY_LOGS(INFO)
          << "G3 WifiLan StopDiscovery: Can't stop discovering because we "
             "never started discovering.";
      return false;
    }
    discovering_info_.Remove(service_type);
  }
  auto& env = MediumEnvironment::Instance();
  env.UpdateWifiLanMediumV2ForDiscovery(*this, {}, service_type, false);
  return true;
}

std::unique_ptr<api::WifiLanSocketV2> WifiLanMediumV2::ConnectToService(
    NsdServiceInfo& remote_service_info, CancellationFlag* cancellation_flag) {
  return nullptr;
}

std::unique_ptr<api::WifiLanSocketV2> WifiLanMediumV2::ConnectToService(
    const std::string& ip_address, int port,
    CancellationFlag* cancellation_flag) {
  return nullptr;
}

std::unique_ptr<api::WifiLanServerSocketV2> WifiLanMediumV2::ListenForService(
    int port) {
  return {};
}

}  // namespace g3
}  // namespace nearby
}  // namespace location
