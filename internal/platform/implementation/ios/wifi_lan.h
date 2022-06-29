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

#ifndef PLATFORM_IMPL_IOS_WIFI_LAN_H_
#define PLATFORM_IMPL_IOS_WIFI_LAN_H_
#ifdef __cplusplus

#import <Foundation/Foundation.h>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#import "internal/platform/implementation/ios/Mediums/GNCMConnection.h"  // IWYU pragma: export
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/nsd_service_info.h"

@class GNCMBonjourBrowser;
@class GNCMBonjourService;

namespace location {
namespace nearby {
namespace ios {

/** InputStream that reads from GNCMConnection. */
class WifiLanInputStream : public InputStream {
 public:
  WifiLanInputStream();
  ~WifiLanInputStream() override;

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;

  GNCMConnectionHandlers* GetConnectionHandlers() { return connectionHandlers_; }

 private:
  GNCMConnectionHandlers* connectionHandlers_;
  NSMutableArray<NSData*>* newDataPackets_;
  NSMutableData* accumulatedData_;
  NSCondition* condition_;
};

/** OutputStream that writes to GNCMConnection. */
class WifiLanOutputStream : public OutputStream {
 public:
  explicit WifiLanOutputStream(id<GNCMConnection> connection)
      : connection_(connection), condition_([[NSCondition alloc] init]) {}
  ~WifiLanOutputStream() override;

  Exception Write(const ByteArray& data) override;
  Exception Flush() override;
  Exception Close() override;

 private:
  id<GNCMConnection> connection_;
  NSCondition* condition_;
};

/** Concrete WifiLanSocket implementation. */
class WifiLanSocket : public api::WifiLanSocket {
 public:
  WifiLanSocket() = default;
  explicit WifiLanSocket(id<GNCMConnection> connection);
  ~WifiLanSocket() override;

  // api::WifiLanSocket:
  InputStream& GetInputStream() override { return *input_stream_; }
  OutputStream& GetOutputStream() override { return *output_stream_; }
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

  bool IsClosed() const ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  void DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
  std::unique_ptr<WifiLanInputStream> input_stream_;
  std::unique_ptr<WifiLanOutputStream> output_stream_;
};

/** Concrete WifiLanServerSocket implementation. */
class WifiLanServerSocket : public api::WifiLanServerSocket {
 public:
  static std::string GetName(const std::string& ip_address, int port);

  ~WifiLanServerSocket() override;

  // api::WifiLanServerSocket:
  std::string GetIPAddress() const override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return ip_address_;
  }
  void SetIPAddress(const std::string& ip_address) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    ip_address_ = ip_address;
  }
  int GetPort() const override ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    return port_;
  }
  void SetPort(int port) ABSL_LOCKS_EXCLUDED(mutex_) {
    absl::MutexLock lock(&mutex_);
    port_ = port;
  }
  std::unique_ptr<api::WifiLanSocket> Accept() override ABSL_LOCKS_EXCLUDED(mutex_);
  Exception Close() override ABSL_LOCKS_EXCLUDED(mutex_);

  bool Connect(std::unique_ptr<WifiLanSocket> socket) ABSL_LOCKS_EXCLUDED(mutex_);
  void SetCloseNotifier(std::function<void()> notifier) ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  Exception DoClose() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  mutable absl::Mutex mutex_;
  std::string ip_address_ ABSL_GUARDED_BY(mutex_);
  int port_ ABSL_GUARDED_BY(mutex_);
  absl::CondVar cond_;
  absl::flat_hash_set<std::unique_ptr<WifiLanSocket>> pending_sockets_ ABSL_GUARDED_BY(mutex_);
  std::function<void()> close_notifier_ ABSL_GUARDED_BY(mutex_);
  bool closed_ ABSL_GUARDED_BY(mutex_) = false;
};

/** Concrete WifiLanMedium implementation. */
class WifiLanMedium : public api::WifiLanMedium {
 public:
  WifiLanMedium() = default;
  ~WifiLanMedium() override;

  WifiLanMedium(const WifiLanMedium&) = delete;
  WifiLanMedium& operator=(const WifiLanMedium&) = delete;

  // Check if a network connection to a primary router exist.
  bool IsNetworkConnected() const override { return true; }

  // api::WifiLanMedium:
  bool StartAdvertising(const NsdServiceInfo& nsd_service_info) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopAdvertising(const NsdServiceInfo& nsd_service_info) override ABSL_LOCKS_EXCLUDED(mutex_);

  bool StartDiscovery(const std::string& service_type, DiscoveredServiceCallback callback) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StopDiscovery(const std::string& service_type) override ABSL_LOCKS_EXCLUDED(mutex_);

  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const NsdServiceInfo& remote_service_info, CancellationFlag* cancellation_flag) override;
  std::unique_ptr<api::WifiLanSocket> ConnectToService(
      const std::string& ip_address, int port, CancellationFlag* cancellation_flag) override;
  std::unique_ptr<api::WifiLanServerSocket> ListenForService(int port) override
      ABSL_LOCKS_EXCLUDED(mutex_);

  absl::optional<std::pair<std::int32_t, std::int32_t>> GetDynamicPortRange() override {
    return absl::nullopt;
  }

 private:
  struct AdvertisingInfo {
    bool Empty() const { return services.empty(); }
    void Clear() { services.clear(); }
    void Add(const std::string& service_type, GNCMBonjourService* service) {
      services.insert({service_type, service});
    }
    void Remove(const std::string& service_type) { services.erase(service_type); }
    bool Existed(const std::string& service_type) const { return services.contains(service_type); }

    absl::flat_hash_map<std::string, GNCMBonjourService*> services;
  };
  struct DiscoveringInfo {
    bool Empty() const { return services.empty(); }
    void Clear() { services.clear(); }
    void Add(const std::string& service_type, GNCMBonjourBrowser* browser) {
      services.insert({service_type, browser});
    }
    void Remove(const std::string& service_type) { services.erase(service_type); }
    bool Existed(const std::string& service_type) const { return services.contains(service_type); }

    absl::flat_hash_map<std::string, GNCMBonjourBrowser*> services;
  };

  std::string GetFakeIPAddress() const;
  int GetFakePort() const;

  absl::Mutex mutex_;
  AdvertisingInfo advertising_info_ ABSL_GUARDED_BY(mutex_);
  int requesting_port_ = 0;
  DiscoveringInfo discovering_info_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, WifiLanServerSocket*> server_sockets_ ABSL_GUARDED_BY(mutex_);
  absl::flat_hash_map<std::string, GNCMConnectionRequester> connection_requesters_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace ios
}  // namespace nearby
}  // namespace location

#endif
#endif  // PLATFORM_IMPL_IOS_WIFI_LAN_H_
