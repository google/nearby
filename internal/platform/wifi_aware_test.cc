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

#include "internal/platform/wifi_aware.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/upgrade_address_info.h"
#include "internal/platform/implementation/wifi_aware.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/wifi_aware_connection_info.h"
#include "proto/connections_enums.pb.h"

namespace nearby {
namespace {

using ::testing::status::StatusIs;
using Medium = ::location::nearby::proto::connections::Medium;

class FakeInputStream : public InputStream {
 public:
  ExceptionOr<ByteArray> Read(std::int64_t size) override {
    return ExceptionOr<ByteArray>(ByteArray("test_data"));
  }
  Exception Close() override { return {Exception::kSuccess}; }
};

class FakeOutputStream : public OutputStream {
 public:
  Exception Write(absl::string_view data) override {
    return {Exception::kSuccess};
  }
  Exception Flush() override { return {Exception::kSuccess}; }
  Exception Close() override { return {Exception::kSuccess}; }
};

class FakeWifiAwareSocket : public api::WifiAwareSocket {
 public:
  InputStream& GetInputStream() override { return input_stream_; }
  OutputStream& GetOutputStream() override { return output_stream_; }
  Exception Close() override {
    closed_ = true;
    return {Exception::kSuccess};
  }
  bool IsClosed() const { return closed_; }

 private:
  FakeInputStream input_stream_;
  FakeOutputStream output_stream_;
  bool closed_ = false;
};

class FakeWifiAwareServerSocket : public api::WifiAwareServerSocket {
 public:
  std::unique_ptr<api::WifiAwareSocket> Accept() override {
    if (closed_) {
      return nullptr;
    }
    return std::make_unique<FakeWifiAwareSocket>();
  }
  Exception Close() override {
    closed_ = true;
    return {Exception::kSuccess};
  }
  bool IsClosed() const { return closed_; }

 private:
  bool closed_ = false;
};

class FakeWifiAwareMedium : public api::WifiAwareMedium {
 public:
  bool StartAdvertising(const NsdServiceInfo& nsd_service_info) override {
    advertising_ = true;
    return true;
  }

  bool StopAdvertising(const NsdServiceInfo& nsd_service_info) override {
    advertising_ = false;
    return true;
  }

  bool StartDiscovery(const std::string& service_type,
                      DiscoveredServiceCallback callback) override {
    if (discovery_fail_) {
      return false;
    }
    discovery_callbacks_[service_type] = std::move(callback);
    return true;
  }

  bool StopDiscovery(const std::string& service_type) override {
    discovery_callbacks_.erase(service_type);
    return true;
  }

  bool IsPublishing() override { return publishing_; }
  bool StartPublishing() override {
    publishing_ = true;
    return true;
  }
  bool StopPublishing() override {
    publishing_ = false;
    return true;
  }

  bool IsSubscribing() override { return subscribing_; }
  bool StartSubscribing() override {
    subscribing_ = true;
    return true;
  }
  bool StopSubscribing() override {
    subscribing_ = false;
    return true;
  }

  std::unique_ptr<api::WifiAwareSocket> ConnectToService(
      const NsdServiceInfo& remote_service_info,
      CancellationFlag* cancellation_flag) override {
    return std::make_unique<FakeWifiAwareSocket>();
  }

  std::unique_ptr<api::WifiAwareServerSocket> ListenForService(
      int port) override {
    return std::make_unique<FakeWifiAwareServerSocket>();
  }

  void TriggerDiscovered(const std::string& service_type,
                         const NsdServiceInfo& info) {
    auto it = discovery_callbacks_.find(service_type);
    if (it != discovery_callbacks_.end()) {
      it->second.service_discovered_cb(info);
    }
  }

  void TriggerLost(const std::string& service_type,
                   const NsdServiceInfo& info) {
    auto it = discovery_callbacks_.find(service_type);
    if (it != discovery_callbacks_.end()) {
      it->second.service_lost_cb(info);
    }
  }

  void SetDiscoveryFail(bool fail) { discovery_fail_ = fail; }
  bool IsAdvertising() const { return advertising_; }

 private:
  bool advertising_ = false;
  bool publishing_ = false;
  bool subscribing_ = false;
  bool discovery_fail_ = false;
  std::map<std::string, DiscoveredServiceCallback> discovery_callbacks_;
};

TEST(WifiAwareSocketTest, DefaultConstructorIsInvalid) {
  WifiAwareSocket socket;
  EXPECT_FALSE(socket.IsValid());
  EXPECT_EQ(socket.GetImpl(), nullptr);
  EXPECT_TRUE(socket.Close().Ok());
}

TEST(WifiAwareSocketTest, ValidSocketOperations) {
  std::unique_ptr<api::WifiAwareSocket> fake_socket =
      std::make_unique<FakeWifiAwareSocket>();
  WifiAwareSocket socket(std::move(fake_socket));
  EXPECT_TRUE(socket.IsValid());
  EXPECT_NE(socket.GetImpl(), nullptr);
  EXPECT_TRUE(socket.Close().Ok());
  EXPECT_TRUE(socket.GetInputStream().Close().Ok());
  EXPECT_TRUE(socket.GetOutputStream().Close().Ok());
}

TEST(WifiAwareServerSocketTest, DefaultConstructorIsInvalid) {
  WifiAwareServerSocket server_socket;
  EXPECT_FALSE(server_socket.IsValid());
  EXPECT_EQ(server_socket.GetImpl(), nullptr);
  EXPECT_TRUE(server_socket.Close().Ok());
  WifiAwareSocket accepted = server_socket.Accept();
  EXPECT_FALSE(accepted.IsValid());
}

TEST(WifiAwareServerSocketTest, ValidServerSocketOperations) {
  std::unique_ptr<api::WifiAwareServerSocket> fake_server =
      std::make_unique<FakeWifiAwareServerSocket>();
  WifiAwareServerSocket server_socket(std::move(fake_server));
  EXPECT_TRUE(server_socket.IsValid());
  EXPECT_NE(server_socket.GetImpl(), nullptr);
  WifiAwareSocket accepted = server_socket.Accept();
  EXPECT_TRUE(accepted.IsValid());
  EXPECT_TRUE(server_socket.Close().Ok());
  WifiAwareSocket accepted_after_close = server_socket.Accept();
  EXPECT_FALSE(accepted_after_close.IsValid());
}

TEST(WifiAwareMediumTest, DefaultPlatformImplementation) {
  WifiAwareMedium medium;
  EXPECT_FALSE(medium.IsValid());
  EXPECT_EQ(medium.GetImpl(), nullptr);

  NsdServiceInfo service_info;
  service_info.SetServiceName("service_test");
  service_info.SetServiceType("_nearby._tcp");
  EXPECT_FALSE(medium.StartAdvertising(service_info));
  EXPECT_FALSE(medium.StopAdvertising(service_info));

  WifiAwareMedium::DiscoveredServiceCallback callback;
  EXPECT_FALSE(medium.StartDiscovery("_nearby._tcp", std::move(callback)));
  EXPECT_FALSE(medium.StopDiscovery("_nearby._tcp"));

  EXPECT_FALSE(medium.IsPublishing());
  EXPECT_FALSE(medium.StartPublishing());
  EXPECT_FALSE(medium.StopPublishing());

  EXPECT_FALSE(medium.IsSubscribing());
  EXPECT_FALSE(medium.StartSubscribing());
  EXPECT_FALSE(medium.StopSubscribing());

  CancellationFlag flag;
  WifiAwareSocket socket = medium.ConnectToService(service_info, &flag);
  EXPECT_FALSE(socket.IsValid());

  WifiAwareServerSocket server_socket = medium.ListenForService(1234);
  EXPECT_FALSE(server_socket.IsValid());
}

TEST(WifiAwareMediumTest, AdvertisingAndPublishing) {
  auto fake_impl = std::make_unique<FakeWifiAwareMedium>();
  FakeWifiAwareMedium* fake_ptr = fake_impl.get();
  WifiAwareMedium medium(std::move(fake_impl));

  EXPECT_TRUE(medium.IsValid());
  EXPECT_NE(medium.GetImpl(), nullptr);
  NsdServiceInfo service_info;
  service_info.SetServiceName("service_test");
  service_info.SetServiceType("_nearby._tcp");

  EXPECT_TRUE(medium.StartAdvertising(service_info));
  EXPECT_TRUE(fake_ptr->IsAdvertising());
  EXPECT_TRUE(medium.StopAdvertising(service_info));
  EXPECT_FALSE(fake_ptr->IsAdvertising());

  EXPECT_FALSE(medium.IsPublishing());
  EXPECT_TRUE(medium.StartPublishing());
  EXPECT_TRUE(medium.IsPublishing());
  EXPECT_TRUE(medium.StopPublishing());
  EXPECT_FALSE(medium.IsPublishing());

  EXPECT_FALSE(medium.IsSubscribing());
  EXPECT_TRUE(medium.StartSubscribing());
  EXPECT_TRUE(medium.IsSubscribing());
  EXPECT_TRUE(medium.StopSubscribing());
  EXPECT_FALSE(medium.IsSubscribing());
}

TEST(WifiAwareMediumTest, DiscoveryFlow) {
  auto fake_impl = std::make_unique<FakeWifiAwareMedium>();
  FakeWifiAwareMedium* fake_ptr = fake_impl.get();
  WifiAwareMedium medium(std::move(fake_impl));

  std::string discovered_name;
  std::string lost_name;
  WifiAwareMedium::DiscoveredServiceCallback callback = {
      .service_discovered_cb =
          [&](NsdServiceInfo info, const std::string& type) {
            discovered_name = info.GetServiceName();
          },
      .service_lost_cb =
          [&](NsdServiceInfo info, const std::string& type) {
            lost_name = info.GetServiceName();
          },
  };

  const std::string service_type = "_test_service._tcp";
  EXPECT_TRUE(medium.StartDiscovery(service_type, std::move(callback)));

  // Starting again with the same service type should fail.
  WifiAwareMedium::DiscoveredServiceCallback duplicate_cb;
  EXPECT_FALSE(medium.StartDiscovery(service_type, std::move(duplicate_cb)));

  // Trigger discovery.
  NsdServiceInfo info1;
  info1.SetServiceName("device_1");
  info1.SetServiceType(service_type);
  fake_ptr->TriggerDiscovered(service_type, info1);
  EXPECT_EQ(discovered_name, "device_1");

  // Trigger duplicate discovery for same service name (should be ignored).
  discovered_name.clear();
  fake_ptr->TriggerDiscovered(service_type, info1);
  EXPECT_TRUE(discovered_name.empty());

  // Trigger discovery for unknown service type in NsdServiceInfo (should be
  // safely ignored).
  NsdServiceInfo unknown_type_info;
  unknown_type_info.SetServiceName("device_x");
  unknown_type_info.SetServiceType("unknown_service_type");
  fake_ptr->TriggerDiscovered(service_type, unknown_type_info);
  EXPECT_TRUE(discovered_name.empty());

  // Trigger service lost for unknown service type in NsdServiceInfo (should be
  // safely ignored).
  fake_ptr->TriggerLost(service_type, unknown_type_info);
  EXPECT_TRUE(lost_name.empty());

  // Trigger discovery for a second service name.
  NsdServiceInfo info2;
  info2.SetServiceName("device_2");
  info2.SetServiceType(service_type);
  fake_ptr->TriggerDiscovered(service_type, info2);
  EXPECT_EQ(discovered_name, "device_2");

  // Trigger service lost for non-existent service name.
  NsdServiceInfo info_unknown;
  info_unknown.SetServiceName("device_unknown");
  info_unknown.SetServiceType(service_type);
  fake_ptr->TriggerLost(service_type, info_unknown);
  EXPECT_TRUE(lost_name.empty());

  // Trigger service lost for device_1.
  fake_ptr->TriggerLost(service_type, info1);
  EXPECT_EQ(lost_name, "device_1");

  // Stop discovery.
  EXPECT_TRUE(medium.StopDiscovery(service_type));
  // Stopping again should return false.
  EXPECT_FALSE(medium.StopDiscovery(service_type));
}

TEST(WifiAwareMediumTest, StartDiscoveryFails) {
  auto fake_impl = std::make_unique<FakeWifiAwareMedium>();
  fake_impl->SetDiscoveryFail(true);
  WifiAwareMedium medium(std::move(fake_impl));

  WifiAwareMedium::DiscoveredServiceCallback callback;
  EXPECT_FALSE(medium.StartDiscovery("_fail._tcp", std::move(callback)));
}

TEST(WifiAwareMediumTest, ConnectAndListen) {
  auto fake_impl = std::make_unique<FakeWifiAwareMedium>();
  WifiAwareMedium medium(std::move(fake_impl));

  NsdServiceInfo service_info;
  service_info.SetServiceName("remote_peer");
  CancellationFlag flag;
  WifiAwareSocket socket = medium.ConnectToService(service_info, &flag);
  EXPECT_TRUE(socket.IsValid());

  WifiAwareServerSocket server_socket = medium.ListenForService(1234);
  EXPECT_TRUE(server_socket.IsValid());

  api::UpgradeAddressInfo upgrade_info =
      medium.GetUpgradeAddressCandidates(server_socket);
  EXPECT_TRUE(upgrade_info.address_candidates.empty());
}

TEST(WifiAwareConnectionInfoTest, MediumTypeAndMembers) {
  const std::string service_id = "test_service_id";
  const std::vector<uint8_t> actions = {0x01, 0x02};
  WifiAwareConnectionInfo info(service_id, actions);

  EXPECT_EQ(info.GetMediumType(), Medium::WIFI_AWARE);
  EXPECT_EQ(info.GetServiceId(), service_id);
  EXPECT_EQ(info.GetActions(), actions);
}

TEST(WifiAwareConnectionInfoTest, Equality) {
  WifiAwareConnectionInfo info1("service_a", {0x01});
  WifiAwareConnectionInfo info2("service_a", {0x02});
  WifiAwareConnectionInfo info3("service_b", {0x01});

  EXPECT_TRUE(info1 == info2);
  EXPECT_FALSE(info1 != info2);
  EXPECT_FALSE(info1 == info3);
  EXPECT_TRUE(info1 != info3);
}

TEST(WifiAwareConnectionInfoTest, SerializationRoundtrip) {
  const std::string service_id = "nearby_wifi_aware";
  const std::vector<uint8_t> actions = {0x0A, 0x0B, 0x0C};
  WifiAwareConnectionInfo info(service_id, actions);

  std::string serialized = info.ToDataElementBytes();
  auto deserialized = WifiAwareConnectionInfo::FromDataElementBytes(serialized);
  ASSERT_OK(deserialized);
  EXPECT_EQ(*deserialized, info);
  EXPECT_EQ(deserialized->GetActions(), actions);
}

TEST(WifiAwareConnectionInfoTest, DeserializationErrors) {
  // Too short (< 5 bytes)
  EXPECT_THAT(WifiAwareConnectionInfo::FromDataElementBytes(""),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(WifiAwareConnectionInfo::FromDataElementBytes("\x14\x01"),
              StatusIs(absl::StatusCode::kInvalidArgument));

  // Invalid data element field type (not 0x14)
  std::string bad_type = {'\x15', '\x03', '\x04', '\x10', '\x00'};
  EXPECT_THAT(WifiAwareConnectionInfo::FromDataElementBytes(bad_type),
              StatusIs(absl::StatusCode::kInvalidArgument));

  // Bad length field (length doesn't match remaining size)
  std::string bad_len = {'\x14', '\x05', '\x04', '\x10', '\x00'};
  EXPECT_THAT(WifiAwareConnectionInfo::FromDataElementBytes(bad_len),
              StatusIs(absl::StatusCode::kInvalidArgument));

  // Wrong medium type (not 0x04)
  std::string bad_medium = {'\x14', '\x03', '\x03', '\x10', '\x00'};
  EXPECT_THAT(WifiAwareConnectionInfo::FromDataElementBytes(bad_medium),
              StatusIs(absl::StatusCode::kInvalidArgument));

  // Missing service ID mask
  std::string no_mask = {'\x14', '\x03', '\x04', '\x00', '\x00'};
  EXPECT_THAT(WifiAwareConnectionInfo::FromDataElementBytes(no_mask),
              StatusIs(absl::StatusCode::kInvalidArgument));

  // Truncated service ID bytes
  std::string truncated = {'\x14', '\x04', '\x04', '\x10', '\x05', 'a'};
  EXPECT_THAT(WifiAwareConnectionInfo::FromDataElementBytes(truncated),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace nearby
