
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_AWDL_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_AWDL_H_

#include <string>
#include <utility>

#include "internal/platform/implementation/awdl.h"
#include "internal/platform/nsd_service_info.h"

@class GNCMBonjourBrowser;
@class GNCMBonjourService;
@class GNCNWFramework;
@class GNCNWFrameworkServerSocket;
@class GNCNWFrameworkSocket;

namespace nearby {
namespace apple {

/**
 * InputStream that reads from GNCMConnection.
 */
class AwdlInputStream : public InputStream {
 public:
  explicit AwdlInputStream(GNCNWFrameworkSocket* socket);
  ~AwdlInputStream() override = default;

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;

 private:
  GNCNWFrameworkSocket* socket_;
};

/**
 * OutputStream that writes to GNCMConnection.
 */
class AwdlOutputStream : public OutputStream {
 public:
  explicit AwdlOutputStream(GNCNWFrameworkSocket* socket);
  ~AwdlOutputStream() override = default;

  Exception Write(const ByteArray& data) override;
  Exception Flush() override;
  Exception Close() override;

 private:
  GNCNWFrameworkSocket* socket_;
};

/**
 * Concrete AwdlSocket implementation.
 */
class AwdlSocket : public api::AwdlSocket {
 public:
  explicit AwdlSocket(GNCNWFrameworkSocket* socket);
  ~AwdlSocket() override = default;

  InputStream& GetInputStream() override;
  OutputStream& GetOutputStream() override;
  Exception Close() override;

 private:
  GNCNWFrameworkSocket* socket_;
  std::unique_ptr<AwdlInputStream> input_stream_;
  std::unique_ptr<AwdlOutputStream> output_stream_;
};

/**
 * Concrete AwdlServerSocket implementation.
 */
class AwdlServerSocket : public api::AwdlServerSocket {
 public:
  explicit AwdlServerSocket(GNCNWFrameworkServerSocket* server_socket);
  ~AwdlServerSocket() override = default;

  std::string GetIPAddress() const override;
  int GetPort() const override;
  std::unique_ptr<api::AwdlSocket> Accept() override;
  Exception Close() override;

 private:
  GNCNWFrameworkServerSocket* server_socket_;
};

/**
 * Concrete AwdlMedium implementation.
 */
class AwdlMedium : public api::AwdlMedium {
 public:
  explicit AwdlMedium();
  ~AwdlMedium() override = default;

  AwdlMedium(const AwdlMedium&) = delete;
  AwdlMedium& operator=(const AwdlMedium&) = delete;

  bool IsNetworkConnected() const override { return true; }
  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange() override {
    return absl::nullopt;
  }

  // void SetIncludePeerToPeerForApplePlatform(bool include_peer_to_peer) override;
  bool StartAdvertising(const NsdServiceInfo& nsd_service_info) override;
  bool StopAdvertising(const NsdServiceInfo& nsd_service_info) override;
  bool StartDiscovery(const std::string& service_type, DiscoveredServiceCallback callback) override;
  bool StopDiscovery(const std::string& service_type) override;
  std::unique_ptr<api::AwdlSocket> ConnectToService(const NsdServiceInfo& remote_service_info,
                                                    CancellationFlag* cancellation_flag) override;
  std::unique_ptr<api::AwdlSocket> ConnectToService(const std::string& ip_address, int port,
                                                    CancellationFlag* cancellation_flag) override;
  std::unique_ptr<api::AwdlServerSocket> ListenForService(int port) override;

 private:
  GNCNWFramework* medium_;
  absl::AnyInvocable<void(NsdServiceInfo)> service_discovered_cb_;
  absl::AnyInvocable<void(NsdServiceInfo)> service_lost_cb_;
};

}  // namespace apple
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_APPLE_AWDL_H_
