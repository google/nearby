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

#ifndef PLATFORM_IMPL_WINDOWS_WIFI_LAN_H_
#define PLATFORM_IMPL_WINDOWS_WIFI_LAN_H_

#include "platform/api/wifi_lan.h"
#include "platform/base/exception.h"
#include "platform/base/input_stream.h"
#include "platform/base/output_stream.h"

namespace location {
namespace nearby {
namespace windows {

// Opaque wrapper over a WifiLan service which contains |NsdServiceInfo|.
class WifiLanService : public api::WifiLanService {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~WifiLanService() override = default;

  // Returns the |NsdServiceInfo| which contains the packed string of
  // |WifiLanServiceInfo| and the endpoint info with named key in a TXTRecord
  // map.
  // The details refer to
  // https://developer.android.com/reference/android/net/nsd/NsdServiceInfo.html.
  // TODO(b/184975123): replace with real implementation.
  NsdServiceInfo GetServiceInfo() const override { return NsdServiceInfo{}; }
};

class WifiLanSocket : public api::WifiLanSocket {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~WifiLanSocket() override;

  // Returns the InputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  // TODO(b/184975123): replace with real implementation.
  InputStream& GetInputStream() override { return fake_input_stream_; }

  // Returns the OutputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  // TODO(b/184975123): replace with real implementation.
  OutputStream& GetOutputStream() override { return fake_output_stream_; }

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  // TODO(b/184975123): replace with real implementation.
  Exception Close() override { return Exception{}; }

  // Returns valid WifiLanService pointer if there is a connection, and
  // nullptr otherwise.
  // TODO(b/184975123): replace with real implementation.
  WifiLanService* GetRemoteWifiLanService() override { return nullptr; }

 private:
  // TODO(b/184975123): replace with real implementation.
  class FakeInputStream : public InputStream {
    ~FakeInputStream() override = default;
    ExceptionOr<ByteArray> Read(std::int64_t size) override {
      return ExceptionOr<ByteArray>(Exception::kFailed);
    }
    Exception Close() override { return {.value = Exception::kFailed}; }
  };
  class FakeOutputStream : public OutputStream {
    ~FakeOutputStream() override = default;

    Exception Write(const ByteArray& data) override {
      return {.value = Exception::kFailed};
    }
    Exception Flush() override { return {.value = Exception::kFailed}; }
    Exception Close() override { return {.value = Exception::kFailed}; }
  };
  FakeInputStream fake_input_stream_;
  FakeOutputStream fake_output_stream_;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiLanMedium : public api::WifiLanMedium {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~WifiLanMedium() override = default;

  // TODO(b/184975123): replace with real implementation.
  bool StartAdvertising(const std::string& service_id,
                        const NsdServiceInfo& nsd_service_info) override {
    return false;
  }
  // TODO(b/184975123): replace with real implementation.
  bool StopAdvertising(const std::string& service_id) override { return false; }

  // Returns true once the WifiLan discovery has been initiated.
  // TODO(b/184975123): replace with real implementation.
  bool StartDiscovery(const std::string& service_id,
                      DiscoveredServiceCallback callback) override {
    return false;
  }

  // Returns true once WifiLan discovery for service_id is well and truly
  // stopped; after this returns, there must be no more invocations of the
  // DiscoveredServiceCallback passed in to StartDiscovery() for service_id.
  // TODO(b/184975123): replace with real implementation.
  bool StopDiscovery(const std::string& service_id) override { return false; }

  // Returns true once WifiLan socket connection requests to service_id can be
  // accepted.
  // TODO(b/184975123): replace with real implementation.
  bool StartAcceptingConnections(const std::string& service_id,
                                 AcceptedConnectionCallback callback) override {
    return false;
  }
  // TODO(b/184975123): replace with real implementation.
  bool StopAcceptingConnections(const std::string& service_id) override {
    return false;
  }

  // Connects to a WifiLan service.
  // On success, returns a new WifiLanSocket.
  // On error, returns nullptr.
  // TODO(b/184975123): replace with real implementation.
  std::unique_ptr<api::WifiLanSocket> Connect(
      api::WifiLanService& wifi_lan_service, const std::string& service_id,
      CancellationFlag* cancellation_flag) override {
    return nullptr;
  }

  // TODO(b/184975123): replace with real implementation.
  WifiLanService* GetRemoteService(const std::string& ip_address,
                                   int port) override {
    return nullptr;
  }

  // TODO(b/184975123): replace with real implementation.
  std::pair<std::string, int> GetServiceAddress(
      const std::string& service_id) override {
    return std::pair<std::string, int>{"Un-implemented", 0};
  }
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_WIFI_LAN_H_
