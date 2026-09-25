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

#ifndef PLATFORM_IMPL_APPLE_WIFI_AWARE_H_
#define PLATFORM_IMPL_APPLE_WIFI_AWARE_H_

#include <memory>
#include <string>

#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/apple/network_utils.h"
#include "internal/platform/implementation/wifi_aware.h"
#include "internal/platform/wifi_aware_service_info.h"

@class GNCNWFramework;
@class GNCWiFiAwareConnectionWrapper;
@class GNCAwareManager;
@class NSObject;
@protocol OS_dispatch_semaphore;

namespace nearby {
namespace apple {

/**
 * InputStream that reads from GNCWiFiAwareConnectionWrapper.
 */
class WifiAwareInputStream : public InputStream {
 public:
  explicit WifiAwareInputStream(GNCWiFiAwareConnectionWrapper* socket);
  ~WifiAwareInputStream() override = default;

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;

 private:
  GNCWiFiAwareConnectionWrapper* socket_;
};

/**
 * OutputStream that writes to GNCWiFiAwareConnectionWrapper.
 */
class WifiAwareOutputStream : public OutputStream {
 public:
  explicit WifiAwareOutputStream(GNCWiFiAwareConnectionWrapper* socket);
  ~WifiAwareOutputStream() override = default;

  Exception Write(absl::string_view data) override;
  Exception Flush() override;
  Exception Close() override;

 private:
  GNCWiFiAwareConnectionWrapper* socket_;
};

/**
 * Concrete WifiAwareSocket implementation.
 */
class WifiAwareSocket : public api::WifiAwareSocket {
 public:
  explicit WifiAwareSocket(GNCWiFiAwareConnectionWrapper* socket);
  ~WifiAwareSocket() override = default;

  InputStream& GetInputStream() override;
  OutputStream& GetOutputStream() override;
  Exception Close() override;

 private:
  GNCWiFiAwareConnectionWrapper* socket_;
  std::unique_ptr<WifiAwareInputStream> input_stream_;
  std::unique_ptr<WifiAwareOutputStream> output_stream_;
};
/**
 * Concrete WifiAwareMedium implementation.
 */
class WifiAwareMedium : public api::WifiAwareMedium {
 public:
  WifiAwareMedium();
  explicit WifiAwareMedium(GNCNWFramework* medium);
  ~WifiAwareMedium() override = default;

  WifiAwareMedium(const WifiAwareMedium&) = delete;
  WifiAwareMedium& operator=(const WifiAwareMedium&) = delete;

  bool StartAdvertising(const WifiAwareServiceInfo& wifi_aware_service_info) override;
  bool StopAdvertising(const WifiAwareServiceInfo& wifi_aware_service_info) override;

  bool StartDiscovery(const std::string& service_type, DiscoveredServiceCallback callback) override;
  bool StopDiscovery(const std::string& service_type) override;

  bool IsPublishing() override;
  bool StartPublishing() override;
  bool StopPublishing() override;

  bool IsSubscribing() override;
  bool StartSubscribing() override;
  bool StopSubscribing() override;

  std::unique_ptr<api::WifiAwareSocket> ConnectToService(
      const WifiAwareServiceInfo& remote_service_info,
      CancellationFlag* cancellation_flag) override;

  std::unique_ptr<api::WifiAwareServerSocket> ListenForService(int port) override;

 private:
  bool is_publishing_ = false;
  bool is_subscribing_ = false;
  GNCNWFramework* medium_;
  GNCAwareManager* aware_manager_;
  DiscoveredServiceCallback service_callback_;
  NSObject<OS_dispatch_semaphore>* publish_pairing_semaphore_ = nullptr;
  NSObject<OS_dispatch_semaphore>* subscribe_pairing_semaphore_ = nullptr;
};

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_WIFI_AWARE_H_
