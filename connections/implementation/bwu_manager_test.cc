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

#include "connections/implementation/bwu_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/connection_options.h"
#include "connections/implementation/analytics/analytics_recorder.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/implementation/endpoint_channel_manager.h"
#include "connections/implementation/endpoint_manager.h"
#include "connections/implementation/fake_bwu_handler.h"
#include "connections/implementation/fake_endpoint_channel.h"
#include "connections/implementation/flags/nearby_connections_feature_flags.h"
#include "connections/implementation/mediums/mediums.h"
#include "connections/implementation/offline_frames.h"
#include "connections/implementation/service_id_constants.h"
#include "connections/listeners.h"
#include "connections/medium_selector.h"
#include "internal/flags/nearby_flags.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/medium_environment.h"
#include "internal/platform/service_address.h"
#include "proto/connections_enums.pb.h"

namespace nearby::connections {
namespace {
using ::location::nearby::connections::BandwidthUpgradeNegotiationFrame;
using ::location::nearby::connections::MediumRole;
using ::location::nearby::connections::OfflineFrame;
using ::location::nearby::connections::OsInfo;
using ::location::nearby::connections::V1Frame;
using ::location::nearby::proto::connections::DisconnectionReason;
using ::nearby::analytics::SafeDisconnectionResult;

constexpr absl::string_view kServiceIdA = "ServiceA";
constexpr absl::string_view kServiceIdB = "ServiceB";
constexpr absl::string_view kEndpointId1 = "Endpoint1";
constexpr absl::string_view kEndpointId2 = "Endpoint2";
constexpr absl::string_view kEndpointId3 = "Endpoint3";
constexpr absl::string_view kEndpointId4 = "Endpoint4";
constexpr absl::string_view kEndpointId5 = "Endpoint5";

BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiHotspotCredentials
CreateWifiHotspotCredentials() {
  BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WifiHotspotCredentials
      credentials;
  credentials.set_ssid("Direct-357a2d8c");
  credentials.set_password("b592f7d3");
  credentials.set_port(1234);
  credentials.set_frequency(2412);
  credentials.set_gateway("123.234.23.1");
  auto* candidate = credentials.mutable_address_candidates()->Add();
  candidate->set_ip_address(std::string(
      "\xfe\x80\\x00\x00\x00\x00\x00\x00\x4d\xb2\xb3\x5c\x22\x03\x98\xa1", 16));
  candidate->set_port(1234);
  candidate = credentials.mutable_address_candidates()->Add();
  candidate->set_ip_address("\x7b\xea\x17\x01");
  candidate->set_port(2412);
  return credentials;
}

OfflineFrame CreateWifiHotspotPathAvailableFrameWithoutClientIntroAck() {
  ExceptionOr<OfflineFrame> hotspot_path_available_frame =
      parser::FromBytes(parser::ForBwuWifiHotspotPathAvailable(
          CreateWifiHotspotCredentials(),
          /*supports_disabling_encryption=*/true));
  OfflineFrame available_frame = hotspot_path_available_frame.result();
  available_frame.mutable_v1()
      ->mutable_bandwidth_upgrade_negotiation()
      ->mutable_upgrade_path_info()
      ->set_supports_client_introduction_ack(false);
  return available_frame;
}

OfflineFrame CreateWifiLanPathAvailableFrameWithoutClientIntroAck() {
  ServiceAddress service_address{
      .address = {'A', 'B', 'C', 'D'},
      .port = 1234,
  };
  ExceptionOr<OfflineFrame> wlan_available =
      parser::FromBytes(parser::ForBwuWifiLanPathAvailable({service_address}));
  OfflineFrame available_frame = wlan_available.result();
  available_frame.mutable_v1()
      ->mutable_bandwidth_upgrade_negotiation()
      ->mutable_upgrade_path_info()
      ->set_supports_client_introduction_ack(false);
  return available_frame;
}

class BwuManagerBaseTest : public ::testing::Test {
 protected:
  void SetUp() override {
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::
            kEnableDynamicRoleSwitch,
        true);
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::kEnableWifiDirect,
        true);
  }

  void TearDown() override {
    NearbyFlags::GetInstance().ResetOverridedValues();
  }
};

TEST_F(BwuManagerBaseTest, InitiateBwu_NeedToSwitchRole_Success) {
  ClientProxy client;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  Mediums mediums;
  BwuManager::Config config;
  config.allow_upgrade_to.SetAll(false);
  absl::flat_hash_map<Medium, std::unique_ptr<BwuHandler>> handlers;
  auto bwu_manager = std::make_unique<BwuManager>(mediums, em, ecm,
                                                  std::move(handlers), config);
  client.SetLocalOsType(OsInfo::APPLE);
  auto channel1 = std::make_unique<FakeEndpointChannel>(
      Medium::BLUETOOTH, std::string(kServiceIdA));
  MediumRole medium_role;
  medium_role.set_support_wifi_hotspot_host(true);
  client.OnConnectionInitiated(
      std::string(kEndpointId1),
      {.remote_endpoint_info = ByteArray("remote endpoint")},
      {.auto_upgrade_bandwidth = false,
       .connection_info =
           {
               .medium_role = {medium_role},
           }},
      {}, "");
  client.OnConnectionAccepted(std::string(kEndpointId1));
  ecm.RegisterChannelForEndpoint(&client, std::string(kEndpointId1),
                                 std::move(channel1));
  bwu_manager->InitiateBwuForEndpoint(&client, std::string(kEndpointId1),
                                      Medium::WIFI_HOTSPOT);
  EXPECT_FALSE(bwu_manager->IsUpgradeOngoing(std::string(kEndpointId1)));

  ecm.UnregisterChannelForEndpoint(kEndpointId1,
                                   DisconnectionReason::LOCAL_DISCONNECTION,
                                   SafeDisconnectionResult::kSafeDisconnection);
  bwu_manager->Shutdown();
}

TEST_F(BwuManagerBaseTest,
       InitiateBwu_NeedToSwitchRole_WindowsAndroid_Success) {
  ClientProxy client;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  Mediums mediums;
  BwuManager::Config config;
  config.allow_upgrade_to.SetAll(false);
  absl::flat_hash_map<Medium, std::unique_ptr<BwuHandler>> handlers;
  auto bwu_manager = std::make_unique<BwuManager>(mediums, em, ecm,
                                                  std::move(handlers), config);

  // Set up local as WINDOWS, remote as ANDROID
  client.SetLocalOsType(OsInfo::WINDOWS);
  OsInfo remote_os_info;
  remote_os_info.set_type(OsInfo::ANDROID);

  auto channel1 = std::make_unique<FakeEndpointChannel>(
      Medium::BLUETOOTH, std::string(kServiceIdA));
  auto* channel1_ptr = channel1.get();

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_direct_group_owner(true);

  client.OnConnectionInitiated(
      std::string(kEndpointId1),
      {.remote_endpoint_info = ByteArray("remote endpoint")},
      {.auto_upgrade_bandwidth = false,
       .connection_info =
           {
               .medium_role = {remote_medium_role},
           }},
      {}, "");
  client.OnConnectionAccepted(std::string(kEndpointId1));
  client.SetRemoteOsInfo(kEndpointId1, remote_os_info);

  ecm.RegisterChannelForEndpoint(&client, std::string(kEndpointId1),
                                 std::move(channel1));

  // Verify that before upgrade, write_timestamp is infinite past
  EXPECT_EQ(channel1_ptr->GetLastWriteTimestamp(), absl::InfinitePast());

  // Initiate BWU for WiFi Direct on Windows, which forces
  // role switch to Android
  bwu_manager->InitiateBwuForEndpoint(&client, std::string(kEndpointId1),
                                      Medium::WIFI_DIRECT);

  // Since role is switched, upgrade is NOT initiated locally, but delegated
  EXPECT_FALSE(bwu_manager->IsUpgradeOngoing(std::string(kEndpointId1)));

  // Verify that an UPGRADE_PATH_REQUEST frame was actually written to
  // the channel
  EXPECT_NE(channel1_ptr->GetLastWriteTimestamp(), absl::InfinitePast());

  ecm.UnregisterChannelForEndpoint(kEndpointId1,
                                   DisconnectionReason::LOCAL_DISCONNECTION,
                                   SafeDisconnectionResult::kSafeDisconnection);
  bwu_manager->Shutdown();
}

TEST_F(BwuManagerBaseTest,
       InitiateBwu_NeedToSwitchRole_WindowsAndroid_NoSwitch_Success) {
  ClientProxy client;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  Mediums mediums;
  BwuManager::Config config;
  config.allow_upgrade_to.SetAll(false);
  config.allow_upgrade_to.wifi_direct = true;
  absl::flat_hash_map<Medium, std::unique_ptr<BwuHandler>> handlers;
  auto fake_wifi_direct = std::make_unique<FakeBwuHandler>(Medium::WIFI_DIRECT);
  auto* fake_wifi_direct_ptr = fake_wifi_direct.get();
  handlers.emplace(Medium::WIFI_DIRECT, std::move(fake_wifi_direct));
  auto bwu_manager = std::make_unique<BwuManager>(mediums, em, ecm,
                                                  std::move(handlers), config);

  // Set up local as WINDOWS, remote as ANDROID
  client.SetLocalOsType(OsInfo::WINDOWS);
  OsInfo remote_os_info;
  remote_os_info.set_type(OsInfo::ANDROID);

  auto channel1 = std::make_unique<FakeEndpointChannel>(
      Medium::BLUETOOTH, std::string(kServiceIdA));

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_direct_group_owner(false);

  client.OnConnectionInitiated(
      std::string(kEndpointId1),
      {.remote_endpoint_info = ByteArray("remote endpoint")},
      {.auto_upgrade_bandwidth = false,
       .connection_info =
           {
               .medium_role = {remote_medium_role},
           }},
      {}, "");
  client.OnConnectionAccepted(std::string(kEndpointId1));
  client.SetRemoteOsInfo(kEndpointId1, remote_os_info);

  ecm.RegisterChannelForEndpoint(&client, std::string(kEndpointId1),
                                 std::move(channel1));

  // Verify that before upgrade, upgrade is not ongoing
  EXPECT_FALSE(bwu_manager->IsUpgradeOngoing(std::string(kEndpointId1)));

  // Initiate BWU for WiFi Direct on Windows. Since remote doesn't support GO,
  // we do not switch roles, so we host/upgrade locally.
  bwu_manager->InitiateBwuForEndpoint(&client, std::string(kEndpointId1),
                                      Medium::WIFI_DIRECT);

  // Upgrade is ongoing locally
  EXPECT_TRUE(bwu_manager->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_EQ(fake_wifi_direct_ptr->handle_initialize_calls().size(), 1u);

  ecm.UnregisterChannelForEndpoint(kEndpointId1,
                                   DisconnectionReason::LOCAL_DISCONNECTION,
                                   SafeDisconnectionResult::kSafeDisconnection);
  bwu_manager->Shutdown();
}

// Apple can only be a Wi-Fi Aware R4 subscriber, so when it is the advertiser
// talking to an Android peer that can publish, it must hand the publisher role
// over instead of hosting the upgrade itself.
TEST_F(BwuManagerBaseTest, InitiateBwu_NeedToSwitchRole_AppleAwareR4_Success) {
  ClientProxy client;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  Mediums mediums;
  BwuManager::Config config;
  config.allow_upgrade_to.SetAll(false);
  config.allow_upgrade_to.wifi_aware_r4 = true;
  absl::flat_hash_map<Medium, std::unique_ptr<BwuHandler>> handlers;
  auto fake_aware_r4 = std::make_unique<FakeBwuHandler>(Medium::WIFI_AWARE_R4);
  auto* fake_aware_r4_ptr = fake_aware_r4.get();
  handlers.emplace(Medium::WIFI_AWARE_R4, std::move(fake_aware_r4));
  auto bwu_manager = std::make_unique<BwuManager>(mediums, em, ecm,
                                                  std::move(handlers), config);

  client.SetLocalOsType(OsInfo::APPLE);
  OsInfo remote_os_info;
  remote_os_info.set_type(OsInfo::ANDROID);

  auto channel1 = std::make_unique<FakeEndpointChannel>(
      Medium::BLUETOOTH, std::string(kServiceIdA));
  auto* channel1_ptr = channel1.get();

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_aware_publisher(true);

  client.OnConnectionInitiated(
      std::string(kEndpointId1),
      {.remote_endpoint_info = ByteArray("remote endpoint"),
       .is_incoming_connection = true},
      {.auto_upgrade_bandwidth = false,
       .connection_info =
           {
               .medium_role = {remote_medium_role},
           }},
      {}, "");
  client.OnConnectionAccepted(std::string(kEndpointId1));
  client.SetRemoteOsInfo(kEndpointId1, remote_os_info);

  ecm.RegisterChannelForEndpoint(&client, std::string(kEndpointId1),
                                 std::move(channel1));

  EXPECT_EQ(channel1_ptr->GetLastWriteTimestamp(), absl::InfinitePast());

  bwu_manager->InitiateBwuForEndpoint(&client, std::string(kEndpointId1),
                                      Medium::WIFI_AWARE_R4);

  // The role was switched, so we delegate instead of hosting: no local upgrade,
  // and an UPGRADE_PATH_REQUEST frame on the wire.
  EXPECT_FALSE(bwu_manager->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_TRUE(fake_aware_r4_ptr->handle_initialize_calls().empty());
  EXPECT_NE(channel1_ptr->GetLastWriteTimestamp(), absl::InfinitePast());

  ecm.UnregisterChannelForEndpoint(kEndpointId1,
                                   DisconnectionReason::LOCAL_DISCONNECTION,
                                   SafeDisconnectionResult::kSafeDisconnection);
  bwu_manager->Shutdown();
}

// When Apple is the discoverer the Android peer is already the publisher, so
// switching would only force a redundant re-publish on a new port.
TEST_F(BwuManagerBaseTest,
       InitiateBwu_NeedToSwitchRole_AppleAwareR4_Outgoing_NoSwitch) {
  ClientProxy client;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  Mediums mediums;
  BwuManager::Config config;
  config.allow_upgrade_to.SetAll(false);
  config.allow_upgrade_to.wifi_aware_r4 = true;
  absl::flat_hash_map<Medium, std::unique_ptr<BwuHandler>> handlers;
  auto fake_aware_r4 = std::make_unique<FakeBwuHandler>(Medium::WIFI_AWARE_R4);
  auto* fake_aware_r4_ptr = fake_aware_r4.get();
  handlers.emplace(Medium::WIFI_AWARE_R4, std::move(fake_aware_r4));
  auto bwu_manager = std::make_unique<BwuManager>(mediums, em, ecm,
                                                  std::move(handlers), config);

  client.SetLocalOsType(OsInfo::APPLE);
  OsInfo remote_os_info;
  remote_os_info.set_type(OsInfo::ANDROID);

  auto channel1 = std::make_unique<FakeEndpointChannel>(
      Medium::BLUETOOTH, std::string(kServiceIdA));

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_aware_publisher(true);

  client.OnConnectionInitiated(
      std::string(kEndpointId1),
      {.remote_endpoint_info = ByteArray("remote endpoint"),
       .is_incoming_connection = false},
      {.auto_upgrade_bandwidth = false,
       .connection_info =
           {
               .medium_role = {remote_medium_role},
           }},
      {}, "");
  client.OnConnectionAccepted(std::string(kEndpointId1));
  client.SetRemoteOsInfo(kEndpointId1, remote_os_info);

  ecm.RegisterChannelForEndpoint(&client, std::string(kEndpointId1),
                                 std::move(channel1));

  bwu_manager->InitiateBwuForEndpoint(&client, std::string(kEndpointId1),
                                      Medium::WIFI_AWARE_R4);

  EXPECT_TRUE(bwu_manager->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_EQ(fake_aware_r4_ptr->handle_initialize_calls().size(), 1u);

  ecm.UnregisterChannelForEndpoint(kEndpointId1,
                                   DisconnectionReason::LOCAL_DISCONNECTION,
                                   SafeDisconnectionResult::kSafeDisconnection);
  bwu_manager->Shutdown();
}

// An Apple peer cannot publish, so Apple <-> Apple must keep the default
// behaviour of hosting locally.
TEST_F(BwuManagerBaseTest,
       InitiateBwu_NeedToSwitchRole_AppleAwareR4_ApplePeer_NoSwitch) {
  ClientProxy client;
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  Mediums mediums;
  BwuManager::Config config;
  config.allow_upgrade_to.SetAll(false);
  config.allow_upgrade_to.wifi_aware_r4 = true;
  absl::flat_hash_map<Medium, std::unique_ptr<BwuHandler>> handlers;
  auto fake_aware_r4 = std::make_unique<FakeBwuHandler>(Medium::WIFI_AWARE_R4);
  auto* fake_aware_r4_ptr = fake_aware_r4.get();
  handlers.emplace(Medium::WIFI_AWARE_R4, std::move(fake_aware_r4));
  auto bwu_manager = std::make_unique<BwuManager>(mediums, em, ecm,
                                                  std::move(handlers), config);

  client.SetLocalOsType(OsInfo::APPLE);
  OsInfo remote_os_info;
  remote_os_info.set_type(OsInfo::APPLE);

  auto channel1 = std::make_unique<FakeEndpointChannel>(
      Medium::BLUETOOTH, std::string(kServiceIdA));

  // Even if the peer claimed it could publish, it is not an Android device.
  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_aware_publisher(true);

  client.OnConnectionInitiated(
      std::string(kEndpointId1),
      {.remote_endpoint_info = ByteArray("remote endpoint"),
       .is_incoming_connection = true},
      {.auto_upgrade_bandwidth = false,
       .connection_info =
           {
               .medium_role = {remote_medium_role},
           }},
      {}, "");
  client.OnConnectionAccepted(std::string(kEndpointId1));
  client.SetRemoteOsInfo(kEndpointId1, remote_os_info);

  ecm.RegisterChannelForEndpoint(&client, std::string(kEndpointId1),
                                 std::move(channel1));

  bwu_manager->InitiateBwuForEndpoint(&client, std::string(kEndpointId1),
                                      Medium::WIFI_AWARE_R4);

  EXPECT_TRUE(bwu_manager->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_EQ(fake_aware_r4_ptr->handle_initialize_calls().size(), 1u);

  ecm.UnregisterChannelForEndpoint(kEndpointId1,
                                   DisconnectionReason::LOCAL_DISCONNECTION,
                                   SafeDisconnectionResult::kSafeDisconnection);
  bwu_manager->Shutdown();
}

class BwuManagerTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    MediumEnvironment::Instance().Start({.wifi_aware_enabled = true});
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::kEnableWifiDirect,
        true);
  }

  static void TearDownTestSuite() {
    NearbyFlags::GetInstance().ResetOverridedValues();
    MediumEnvironment::Instance().Stop();
  }

  BwuManagerTest() {
    ReinitBwuManager(BooleanMediumSelector{.web_rtc = true,
                                           .wifi_lan = true,
                                           .wifi_hotspot = true,
                                           .wifi_direct = true});
  }

  ~BwuManagerTest() override {
    if (bwu_manager_ != nullptr) {
      bwu_manager_->Shutdown();
    }
  }

  void TearDown() override {
    NearbyFlags::GetInstance().ResetOverridedValues();
    NearbyFlags::GetInstance().OverrideBoolFlagValue(
        config_package_nearby::nearby_connections_feature::kEnableWifiDirect,
        true);
    MediumEnvironment::Instance().Stop();
    MediumEnvironment::Instance().Start({.wifi_aware_enabled = true});
  }

  void ReinitBwuManager(BooleanMediumSelector allow_upgrade_to) {
    if (bwu_manager_ != nullptr) {
      bwu_manager_->Shutdown();
      bwu_manager_.reset();
    }
    mediums_ = std::make_unique<Mediums>();
    fake_web_rtc_bwu_handler_ = nullptr;
    fake_wifi_lan_bwu_handler_ = nullptr;
    fake_wifi_direct_bwu_handler_ = nullptr;
    fake_wifi_hotspot_bwu_handler_ = nullptr;
    fake_wifi_aware_r4_bwu_handler_ = nullptr;

    absl::flat_hash_map<Medium, std::unique_ptr<BwuHandler>> handlers;
    if (allow_upgrade_to.web_rtc) {
      auto fake_web_rtc = std::make_unique<FakeBwuHandler>(Medium::WEB_RTC);
      fake_web_rtc_bwu_handler_ = fake_web_rtc.get();
      handlers.emplace(Medium::WEB_RTC, std::move(fake_web_rtc));
    }
    if (allow_upgrade_to.wifi_lan) {
      auto fake_wifi_lan = std::make_unique<FakeBwuHandler>(Medium::WIFI_LAN);
      fake_wifi_lan_bwu_handler_ = fake_wifi_lan.get();
      handlers.emplace(Medium::WIFI_LAN, std::move(fake_wifi_lan));
    }
    if (allow_upgrade_to.wifi_direct) {
      auto fake_wifi_direct =
          std::make_unique<FakeBwuHandler>(Medium::WIFI_DIRECT);
      fake_wifi_direct_bwu_handler_ = fake_wifi_direct.get();
      handlers.emplace(Medium::WIFI_DIRECT, std::move(fake_wifi_direct));
    }
    if (allow_upgrade_to.wifi_hotspot) {
      auto fake_wifi_hotspot =
          std::make_unique<FakeBwuHandler>(Medium::WIFI_HOTSPOT);
      fake_wifi_hotspot_bwu_handler_ = fake_wifi_hotspot.get();
      handlers.emplace(Medium::WIFI_HOTSPOT, std::move(fake_wifi_hotspot));
    }
    if (allow_upgrade_to.wifi_aware || allow_upgrade_to.wifi_aware_r4) {
      auto fake_wifi_aware =
          std::make_unique<FakeBwuHandler>(Medium::WIFI_AWARE_R4);
      fake_wifi_aware_r4_bwu_handler_ = fake_wifi_aware.get();
      handlers.emplace(Medium::WIFI_AWARE_R4, std::move(fake_wifi_aware));
    }

    BwuManager::Config config;
    config.allow_upgrade_to = allow_upgrade_to;
    bwu_manager_ = std::make_unique<BwuManager>(*mediums_, em_, ecm_,
                                                std::move(handlers), config);
    // Don't run tasks on other threads. Avoids race conditions in tests.
    bwu_manager_->MakeSingleThreadedForTesting();
  }

  FakeEndpointChannel* CreateConnectedEndpoint(
      absl::string_view endpoint_id, const MediumRole& remote_medium_role,
      BooleanMediumSelector allowed,
      std::optional<OsInfo::OsType> remote_os_type = std::nullopt,
      bool is_incoming_connection = false) {
    ConnectionResponseInfo response_info{
        .remote_endpoint_info = ByteArray("remote endpoint"),
        .authentication_token = "auth_token",
        .raw_authentication_token = ByteArray("auth_token"),
        .is_incoming_connection = is_incoming_connection,
    };
    ConnectionOptions connection_options{
        .auto_upgrade_bandwidth = false,
        .connection_info = {.medium_role = remote_medium_role},
    };
    connection_options.allowed = allowed;
    auto channel = std::make_unique<FakeEndpointChannel>(
        Medium::BLUETOOTH, std::string(kServiceIdA));
    FakeEndpointChannel* channel_raw = channel.get();
    ecm_.RegisterChannelForEndpoint(&client_, std::string(endpoint_id),
                                    std::move(channel));
    client_.OnConnectionInitiated(std::string(endpoint_id), response_info,
                                  connection_options, {}, "token");
    if (is_incoming_connection) {
      client_.LocalEndpointAcceptedConnection(std::string(endpoint_id), {});
      client_.RemoteEndpointAcceptedConnection(std::string(endpoint_id));
    }
    client_.OnConnectionAccepted(std::string(endpoint_id));
    if (remote_os_type.has_value()) {
      OsInfo remote_os_info;
      remote_os_info.set_type(*remote_os_type);
      client_.SetRemoteOsInfo(endpoint_id, remote_os_info);
    }
    return channel_raw;
  }

  void SetSupportMultipleBwuMediums(bool support_multiple_bwu_mediums) {
    FeatureFlags& feature_flags = FeatureFlags::GetMutableInstanceForTesting();
    FeatureFlags::Flags flags = feature_flags.GetFlags();
    flags.support_multiple_bwu_mediums = support_multiple_bwu_mediums;
    feature_flags.SetFlags(flags);
  }

  // Create the initial device-to-device connection, before bandwidth upgrade.
  // Typically |medium| will be Bluetooth.
  FakeEndpointChannel* CreateInitialEndpoint(ClientProxy* client,
                                             absl::string_view service_id,
                                             absl::string_view endpoint_id,
                                             Medium medium) {
    client->OnConnectionInitiated(
        std::string(endpoint_id),
        {.remote_endpoint_info = ByteArray("remote endpoint")},
        {.auto_upgrade_bandwidth = false}, {}, "");
    client->OnConnectionAccepted(std::string(endpoint_id));
    auto channel =
        std::make_unique<FakeEndpointChannel>(medium, std::string(service_id));
    FakeEndpointChannel* channel_raw = channel.get();
    ecm_.RegisterChannelForEndpoint(&client_, std::string(endpoint_id),
                                    std::move(channel));
    return channel_raw;
  }
  void UnRegisterChannelForEndpoint(absl::string_view endpoint_id) {
    ecm_.UnregisterChannelForEndpoint(
        endpoint_id, DisconnectionReason::LOCAL_DISCONNECTION,
        SafeDisconnectionResult::kSafeDisconnection);
  }

  // Upgrade from |initial_medium| to |upgrade_medium|, close down the BLUETOOTH
  // channel, return the upgraded endpoint channel. This logic is tested in
  // InitiateBwu_Success; we use this function in subsequent tests for
  // convenience.
  FakeEndpointChannel* FullyUpgradeEndpoint(absl::string_view endpoint_id,
                                            Medium initial_medium,
                                            Medium upgrade_medium) {
    FakeBwuHandler* handler = nullptr;
    switch (upgrade_medium) {
      case Medium::WEB_RTC:
        handler = fake_web_rtc_bwu_handler_;
        break;
      case Medium::WIFI_LAN:
        handler = fake_wifi_lan_bwu_handler_;
        break;
      case Medium::WIFI_DIRECT:
        handler = fake_wifi_direct_bwu_handler_;
        break;
      case Medium::WIFI_HOTSPOT:
        handler = fake_wifi_hotspot_bwu_handler_;
        break;
      default:
        return nullptr;
    }

    // Create upgraded channel.
    bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(endpoint_id),
                                         upgrade_medium);
    FakeEndpointChannel* upgraded_channel =
        handler->NotifyBwuManagerOfIncomingConnection(
            handler->handle_initialize_calls().size() - 1, bwu_manager_.get());

    // Close initial channel.
    ExceptionOr<OfflineFrame> last_write_frame =
        parser::FromBytes(parser::ForBwuLastWrite());
    bwu_manager_->OnIncomingFrame(last_write_frame.result(),
                                  std::string(endpoint_id), &client_,
                                  initial_medium);
    ExceptionOr<OfflineFrame> safe_to_close_frame =
        parser::FromBytes(parser::ForBwuSafeToClose());
    bwu_manager_->OnIncomingFrame(safe_to_close_frame.result(),
                                  std::string(endpoint_id), &client_,
                                  initial_medium);

    return upgraded_channel;
  }

  ClientProxy client_;
  EndpointChannelManager ecm_;
  EndpointManager em_{&ecm_};
  // It's okay there are no actual Mediums (i.e., implementations). These won't
  // be needed if we pass in an explict medium to InitiateBwuForEndpoint.
  std::unique_ptr<Mediums> mediums_;
  FakeBwuHandler* fake_web_rtc_bwu_handler_ = nullptr;
  FakeBwuHandler* fake_wifi_lan_bwu_handler_ = nullptr;
  FakeBwuHandler* fake_wifi_direct_bwu_handler_ = nullptr;
  FakeBwuHandler* fake_wifi_hotspot_bwu_handler_ = nullptr;
  FakeBwuHandler* fake_wifi_aware_r4_bwu_handler_ = nullptr;
  std::unique_ptr<BwuManager> bwu_manager_;
};

TEST_F(BwuManagerTest, AllowToUpgradeMedium) {
  auto channel1 = std::make_unique<FakeEndpointChannel>(
      Medium::BLUETOOTH, std::string(kServiceIdA));
  ecm_.RegisterChannelForEndpoint(&client_, std::string(kEndpointId1),
                                  std::move(channel1));
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WIFI_LAN);
  EXPECT_TRUE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  ecm_.UnregisterChannelForEndpoint(
      kEndpointId1, DisconnectionReason::LOCAL_DISCONNECTION,
      SafeDisconnectionResult::kSafeDisconnection);

  auto channel2 = std::make_unique<FakeEndpointChannel>(
      Medium::BLUETOOTH, std::string(kServiceIdA));
  ecm_.RegisterChannelForEndpoint(&client_, std::string(kEndpointId2),
                                  std::move(channel2));
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId2),
                                       Medium::WIFI_HOTSPOT);
  EXPECT_TRUE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId2)));
  ecm_.UnregisterChannelForEndpoint(
      kEndpointId2, DisconnectionReason::LOCAL_DISCONNECTION,
      SafeDisconnectionResult::kSafeDisconnection);

  auto channel3 = std::make_unique<FakeEndpointChannel>(
      Medium::BLUETOOTH, std::string(kServiceIdA));
  ecm_.RegisterChannelForEndpoint(&client_, std::string(kEndpointId3),
                                  std::move(channel3));
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId3),
                                       Medium::WIFI_DIRECT);
  EXPECT_TRUE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId3)));
  ecm_.UnregisterChannelForEndpoint(
      kEndpointId3, DisconnectionReason::LOCAL_DISCONNECTION,
      SafeDisconnectionResult::kSafeDisconnection);

  auto channel4 = std::make_unique<FakeEndpointChannel>(
      Medium::WEB_RTC, std::string(kServiceIdA));
  ecm_.RegisterChannelForEndpoint(&client_, std::string(kEndpointId4),
                                  std::move(channel4));
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId4),
                                       Medium::BLUETOOTH);
  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId4)));
  ecm_.UnregisterChannelForEndpoint(
      kEndpointId4, DisconnectionReason::LOCAL_DISCONNECTION,
      SafeDisconnectionResult::kSafeDisconnection);
}

TEST_F(BwuManagerTest,
       InitiateBwu_Revert_OnDisconnect_MultipleEndpoints_FlagEnabled) {
  SetSupportMultipleBwuMediums(true);

  // Say we have two already upgraded WebRTC connections for the same service.
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId2, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);
  FullyUpgradeEndpoint(kEndpointId2, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);

  std::string upgrade_service_id = WrapInitiatorUpgradeServiceId(kServiceIdA);

  EXPECT_TRUE(fake_web_rtc_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_web_rtc_bwu_handler_->handle_revert_calls().empty());
  {
    // Disconnect the first WebRTC endpoint. We don't expect a revert until the
    // last WebRTC endpoint for the service is disconnected.
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(
        kEndpointId1, DisconnectionReason::LOCAL_DISCONNECTION,
        SafeDisconnectionResult::kUnsafeDisconnection);
    bwu_manager_->OnEndpointDisconnect(
        &client_, upgrade_service_id, std::string(kEndpointId1), latch,
        DisconnectionReason::LOCAL_DISCONNECTION);
    ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId1,
              fake_web_rtc_bwu_handler_->disconnect_calls()[0].endpoint_id);
    EXPECT_TRUE(fake_web_rtc_bwu_handler_->handle_revert_calls().empty());
  }
  {
    // Disconnect the second WebRTC endpoint. We expect a revert.
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(
        kEndpointId2, DisconnectionReason::LOCAL_DISCONNECTION,
        SafeDisconnectionResult::kUnsafeDisconnection);
    bwu_manager_->OnEndpointDisconnect(
        &client_, upgrade_service_id, std::string(kEndpointId2), latch,
        DisconnectionReason::LOCAL_DISCONNECTION);
    ASSERT_EQ(2u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId2,
              fake_web_rtc_bwu_handler_->disconnect_calls()[1].endpoint_id);
    ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(upgrade_service_id,
              fake_web_rtc_bwu_handler_->handle_revert_calls()[0].service_id);
  }
}

TEST_F(BwuManagerTest,
       InitiateBwu_Revert_OnDisconnect_MultipleEndpoints_FlagDisabled) {
  SetSupportMultipleBwuMediums(false);

  // Say we have two already upgraded WebRTC connections for the same service.
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId2, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);
  FullyUpgradeEndpoint(kEndpointId2, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);

  std::string upgrade_service_id = WrapInitiatorUpgradeServiceId(kServiceIdA);

  EXPECT_TRUE(fake_web_rtc_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_web_rtc_bwu_handler_->handle_revert_calls().empty());
  {
    // Disconnect the first WebRTC endpoint.
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(
        kEndpointId1, DisconnectionReason::LOCAL_DISCONNECTION,
        SafeDisconnectionResult::kUnsafeDisconnection);
    bwu_manager_->OnEndpointDisconnect(
        &client_, upgrade_service_id, std::string(kEndpointId1), latch,
        DisconnectionReason::LOCAL_DISCONNECTION);
    ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId1,
              fake_web_rtc_bwu_handler_->disconnect_calls()[0].endpoint_id);

    // Note(nohle): There appears to be an off-by-one error in the
    // existing/flag-disabled code. Revert is called when there are "<= 1"
    // (instead of "== 0") connected endpoints.
    ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(upgrade_service_id,
              fake_web_rtc_bwu_handler_->handle_revert_calls()[0].service_id);
  }
  {
    // Disconnect the second WebRTC endpoint.
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(
        kEndpointId2, DisconnectionReason::LOCAL_DISCONNECTION,
        SafeDisconnectionResult::kUnsafeDisconnection);
    bwu_manager_->OnEndpointDisconnect(
        &client_, upgrade_service_id, std::string(kEndpointId2), latch,
        DisconnectionReason::LOCAL_DISCONNECTION);

    // Note(nohle): There appears to be an off-by-one error in the
    // existing/flag-disabled code. Revert is called when there are "<= 1"
    // (instead of "== 0") connected endpoints.
    // The WebRTC medium was already reverted, so we don't expect any more
    // disconnect or revert calls to be processed for WebRTC.
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());
  }
}

TEST_F(BwuManagerTest,
       InitiateBwu_Revert_OnDisconnect_MultipleServices_FlagEnabled) {
  SetSupportMultipleBwuMediums(true);

  // Say we have two already upgraded WLAN connections for different services.
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  CreateInitialEndpoint(&client_, kServiceIdB, kEndpointId2, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_LAN);
  FullyUpgradeEndpoint(kEndpointId2, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_LAN);

  std::string upgrade_service_id_A = WrapInitiatorUpgradeServiceId(kServiceIdA);
  std::string upgrade_service_id_B = WrapInitiatorUpgradeServiceId(kServiceIdB);

  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_revert_calls().empty());
  {
    CountDownLatch latch(1);
    EXPECT_EQ(2u, ecm_.GetConnectedEndpointsCount());
    ecm_.UnregisterChannelForEndpoint(
        kEndpointId1, DisconnectionReason::LOCAL_DISCONNECTION,
        SafeDisconnectionResult::kUnsafeDisconnection);
    EXPECT_EQ(1u, ecm_.GetConnectedEndpointsCount());
    bwu_manager_->OnEndpointDisconnect(
        &client_, upgrade_service_id_A, std::string(kEndpointId1), latch,
        DisconnectionReason::LOCAL_DISCONNECTION);
    ASSERT_EQ(1u, fake_wifi_lan_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId1,
              fake_wifi_lan_bwu_handler_->disconnect_calls()[0].endpoint_id);

    // With the support_multiple_bwu_mediums flag enabled, we have more
    // granular per-service tracking. So, we can revert for each service when
    // the last endpoint of that medium for the service goes down.
    ASSERT_EQ(1u, fake_wifi_lan_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(upgrade_service_id_A,
              fake_wifi_lan_bwu_handler_->handle_revert_calls()[0].service_id);
  }
  {
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(
        kEndpointId2, DisconnectionReason::LOCAL_DISCONNECTION,
        SafeDisconnectionResult::kUnsafeDisconnection);
    EXPECT_EQ(0u, ecm_.GetConnectedEndpointsCount());
    bwu_manager_->OnEndpointDisconnect(
        &client_, upgrade_service_id_B, std::string(kEndpointId2), latch,
        DisconnectionReason::LOCAL_DISCONNECTION);
    ASSERT_EQ(2u, fake_wifi_lan_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId2,
              fake_wifi_lan_bwu_handler_->disconnect_calls()[1].endpoint_id);
    EXPECT_EQ(2u, fake_wifi_lan_bwu_handler_->handle_revert_calls().size());
  }
}

TEST_F(BwuManagerTest,
       InitiateBwu_Revert_OnDisconnect_MultipleServices_FlagDisabled) {
  SetSupportMultipleBwuMediums(false);

  // Say we have two already upgraded WLAN connections for different services.
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  CreateInitialEndpoint(&client_, kServiceIdB, kEndpointId2, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_LAN);
  FullyUpgradeEndpoint(kEndpointId2, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_LAN);

  std::string upgrade_service_id_A = WrapInitiatorUpgradeServiceId(kServiceIdA);
  std::string upgrade_service_id_B = WrapInitiatorUpgradeServiceId(kServiceIdB);

  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_revert_calls().empty());
  {
    CountDownLatch latch(1);
    EXPECT_EQ(2u, ecm_.GetConnectedEndpointsCount());
    ecm_.UnregisterChannelForEndpoint(
        kEndpointId1, DisconnectionReason::LOCAL_DISCONNECTION,
        SafeDisconnectionResult::kUnsafeDisconnection);
    EXPECT_EQ(1u, ecm_.GetConnectedEndpointsCount());
    bwu_manager_->OnEndpointDisconnect(
        &client_, upgrade_service_id_A, std::string(kEndpointId1), latch,
        DisconnectionReason::LOCAL_DISCONNECTION);
    ASSERT_EQ(1u, fake_wifi_lan_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId1,
              fake_wifi_lan_bwu_handler_->disconnect_calls()[0].endpoint_id);

    // With the flag disabled, we only look at the _total_ number of connected
    // endpoints and revert _all_ services when all endpoints are
    // disconnected. Note(nohle): There appears to be an off-by-one error in
    // the existing/flag-disabled code. Revert is called when there are "<= 1"
    // (instead of "== 0") connected endpoints.
    EXPECT_EQ(2u, fake_wifi_lan_bwu_handler_->handle_revert_calls().size());
  }
  {
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(
        kEndpointId2, DisconnectionReason::LOCAL_DISCONNECTION,
        SafeDisconnectionResult::kUnsafeDisconnection);
    EXPECT_EQ(0u, ecm_.GetConnectedEndpointsCount());
    bwu_manager_->OnEndpointDisconnect(
        &client_, upgrade_service_id_B, std::string(kEndpointId2), latch,
        DisconnectionReason::LOCAL_DISCONNECTION);
    // Note(nohle): There appears to be an off-by-one error in the
    // existing/flag-disabled code. Revert is called when there are "<= 1"
    // (instead of "== 0") connected endpoints.
    // We already reverted for _all_ services.
    EXPECT_EQ(1u, fake_wifi_lan_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(2u, fake_wifi_lan_bwu_handler_->handle_revert_calls().size());
  }
}

TEST_F(
    BwuManagerTest,
    InitiateBwu_Revert_OnDisconnect_MultipleServicesAndEndpoints_FlagEnabled) {
  // Need support_multiple_bwu_mediums_ to run this test with multiple mediums.
  SetSupportMultipleBwuMediums(true);

  // Say we have three upgraded connections for two different services and two
  // different mediums.
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId2, Medium::BLUETOOTH);
  CreateInitialEndpoint(&client_, kServiceIdB, kEndpointId3, Medium::BLUETOOTH);
  CreateInitialEndpoint(&client_, kServiceIdB, kEndpointId4, Medium::BLUETOOTH);
  CreateInitialEndpoint(&client_, kServiceIdB, kEndpointId5, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);
  FullyUpgradeEndpoint(kEndpointId4, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_HOTSPOT);
  FullyUpgradeEndpoint(kEndpointId5, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_DIRECT);
  FullyUpgradeEndpoint(kEndpointId2, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_LAN);
  FullyUpgradeEndpoint(kEndpointId3, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WIFI_LAN);

  std::string upgrade_service_id_A = WrapInitiatorUpgradeServiceId(kServiceIdA);
  std::string upgrade_service_id_B = WrapInitiatorUpgradeServiceId(kServiceIdB);

  // Verify that the medium's BWU handler only gets notified to revert the
  // medium--for example, stop accepting connections on the socket--once every
  // endpoint of that medium for the service is disconnected.
  EXPECT_TRUE(fake_web_rtc_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_wifi_hotspot_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_wifi_direct_bwu_handler_->disconnect_calls().empty());
  EXPECT_TRUE(fake_web_rtc_bwu_handler_->handle_revert_calls().empty());
  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_revert_calls().empty());
  EXPECT_TRUE(fake_wifi_hotspot_bwu_handler_->handle_revert_calls().empty());
  EXPECT_TRUE(fake_wifi_direct_bwu_handler_->handle_revert_calls().empty());
  {
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(
        kEndpointId1, DisconnectionReason::LOCAL_DISCONNECTION,
        SafeDisconnectionResult::kUnsafeDisconnection);
    bwu_manager_->OnEndpointDisconnect(
        &client_, upgrade_service_id_A, std::string(kEndpointId1), latch,
        DisconnectionReason::LOCAL_DISCONNECTION);

    // No more WebRTC channels for service A; expect revert call.
    ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId1,
              fake_web_rtc_bwu_handler_->disconnect_calls()[0].endpoint_id);
    ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(upgrade_service_id_A,
              fake_web_rtc_bwu_handler_->handle_revert_calls()[0].service_id);

    // We reverted a WebRTC channel; no WLAN calls expected.
    EXPECT_TRUE(fake_wifi_lan_bwu_handler_->disconnect_calls().empty());
    EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_revert_calls().empty());
    EXPECT_TRUE(fake_wifi_hotspot_bwu_handler_->disconnect_calls().empty());
    EXPECT_TRUE(fake_wifi_hotspot_bwu_handler_->handle_revert_calls().empty());
    EXPECT_TRUE(fake_wifi_direct_bwu_handler_->disconnect_calls().empty());
    EXPECT_TRUE(fake_wifi_direct_bwu_handler_->handle_revert_calls().empty());
  }
  {
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(
        kEndpointId2, DisconnectionReason::LOCAL_DISCONNECTION,
        SafeDisconnectionResult::kUnsafeDisconnection);
    bwu_manager_->OnEndpointDisconnect(
        &client_, upgrade_service_id_A, std::string(kEndpointId2), latch,
        DisconnectionReason::LOCAL_DISCONNECTION);

    // We reverted a WLAN channel; no additional WebRTC calls expected.
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());

    // No more WLAN channels for service A; expect revert call.
    ASSERT_EQ(1u, fake_wifi_lan_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId2,
              fake_wifi_lan_bwu_handler_->disconnect_calls()[0].endpoint_id);
    ASSERT_EQ(1u, fake_wifi_lan_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(upgrade_service_id_A,
              fake_wifi_lan_bwu_handler_->handle_revert_calls()[0].service_id);
  }
  {
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(
        kEndpointId3, DisconnectionReason::LOCAL_DISCONNECTION,
        SafeDisconnectionResult::kUnsafeDisconnection);
    bwu_manager_->OnEndpointDisconnect(
        &client_, upgrade_service_id_B, std::string(kEndpointId3), latch,
        DisconnectionReason::LOCAL_DISCONNECTION);

    // We reverted a WLAN channel; no additional WebRTC calls expected.
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());

    // No more WLAN channels for service B; expect revert call.
    ASSERT_EQ(2u, fake_wifi_lan_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId3,
              fake_wifi_lan_bwu_handler_->disconnect_calls()[1].endpoint_id);
    ASSERT_EQ(2u, fake_wifi_lan_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(upgrade_service_id_B,
              fake_wifi_lan_bwu_handler_->handle_revert_calls()[1].service_id);
  }
  {
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(
        kEndpointId4, DisconnectionReason::LOCAL_DISCONNECTION,
        SafeDisconnectionResult::kUnsafeDisconnection);
    bwu_manager_->OnEndpointDisconnect(
        &client_, upgrade_service_id_B, std::string(kEndpointId4), latch,
        DisconnectionReason::LOCAL_DISCONNECTION);

    // We reverted a Hotspot channel; no additional WebRTC calls expected.
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());

    // No more Hotspot channels for service B; expect revert call.
    ASSERT_EQ(1u, fake_wifi_hotspot_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(
        kEndpointId4,
        fake_wifi_hotspot_bwu_handler_->disconnect_calls()[0].endpoint_id);
    ASSERT_EQ(1u, fake_wifi_hotspot_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(
        upgrade_service_id_B,
        fake_wifi_hotspot_bwu_handler_->handle_revert_calls()[0].service_id);
  }
  {
    CountDownLatch latch(1);
    ecm_.UnregisterChannelForEndpoint(
        kEndpointId5, DisconnectionReason::LOCAL_DISCONNECTION,
        SafeDisconnectionResult::kUnsafeDisconnection);
    bwu_manager_->OnEndpointDisconnect(
        &client_, upgrade_service_id_B, std::string(kEndpointId5), latch,
        DisconnectionReason::LOCAL_DISCONNECTION);

    // We reverted a WifiDirect channel; no additional WebRTC calls expected.
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());

    // No more WifiDirect channels for service B; expect revert call.
    ASSERT_EQ(1u, fake_wifi_direct_bwu_handler_->disconnect_calls().size());
    EXPECT_EQ(kEndpointId5,
              fake_wifi_direct_bwu_handler_->disconnect_calls()[0].endpoint_id);
    ASSERT_EQ(1u, fake_wifi_direct_bwu_handler_->handle_revert_calls().size());
    EXPECT_EQ(
        upgrade_service_id_B,
        fake_wifi_direct_bwu_handler_->handle_revert_calls()[0].service_id);
  }
}

TEST_F(BwuManagerTest, InitiateBwu_Revert_OnUpgradeFailure_FlagEnabled) {
  SetSupportMultipleBwuMediums(true);

  // Say we have two already upgraded WebRTC connections for service A.
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId2, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);
  FullyUpgradeEndpoint(kEndpointId2, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);

  // Service B has an initial Bluetooth connection that it tries to upgrade.
  CreateInitialEndpoint(&client_, kServiceIdB, kEndpointId3, Medium::BLUETOOTH);
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId3),
                                       Medium::WEB_RTC);
  fake_web_rtc_bwu_handler_->NotifyBwuManagerOfIncomingConnection(
      /*initialize_call_index=*/2u, bwu_manager_.get());

  // This upgrade fails.
  BandwidthUpgradeNegotiationFrame::UpgradePathInfo info;
  info.set_medium(BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WEB_RTC);
  ExceptionOr<OfflineFrame> upgrade_failure =
      parser::FromBytes(parser::ForBwuFailure(info));
  bwu_manager_->OnIncomingFrame(upgrade_failure.result(),
                                std::string(kEndpointId3), &client_,
                                Medium::WEB_RTC);

  // With the flag enabled, we can safely revert WebRTC just for service B
  // because service B has no active WebRTC endpoints.
  ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->handle_revert_calls().size());
  EXPECT_EQ(WrapInitiatorUpgradeServiceId(kServiceIdB),
            fake_web_rtc_bwu_handler_->handle_revert_calls()[0].service_id);
  UnRegisterChannelForEndpoint(kEndpointId1);
  UnRegisterChannelForEndpoint(kEndpointId2);
  UnRegisterChannelForEndpoint(kEndpointId3);
}

TEST_F(BwuManagerTest, InitiateBwu_Revert_OnUpgradeFailure_FlagDisabled) {
  SetSupportMultipleBwuMediums(false);

  // Say we have two already upgraded WebRTC connections for service A.
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId2, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);
  FullyUpgradeEndpoint(kEndpointId2, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);

  // Service B has an initial Bluetooth connection that it tries to upgrade.
  CreateInitialEndpoint(&client_, kServiceIdB, kEndpointId3, Medium::BLUETOOTH);
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId3),
                                       Medium::WEB_RTC);
  fake_web_rtc_bwu_handler_->NotifyBwuManagerOfIncomingConnection(
      /*initialize_call_index=*/2u, bwu_manager_.get());

  // This upgrade fails.
  BandwidthUpgradeNegotiationFrame::UpgradePathInfo info;
  info.set_medium(BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WEB_RTC);
  ExceptionOr<OfflineFrame> upgrade_failure =
      parser::FromBytes(parser::ForBwuFailure(info));
  bwu_manager_->OnIncomingFrame(upgrade_failure.result(),
                                std::string(kEndpointId3), &client_,
                                Medium::WEB_RTC);

  // With the flag disabled, we don't revert if there are still connected
  // endpoints for _any_ service. We don't have service-level bookkeeping; we
  // only know that there is some active WebRTC endpoint.
  EXPECT_TRUE(fake_web_rtc_bwu_handler_->handle_revert_calls().empty());
  UnRegisterChannelForEndpoint(kEndpointId1);
  UnRegisterChannelForEndpoint(kEndpointId2);
  UnRegisterChannelForEndpoint(kEndpointId3);
}

TEST_F(BwuManagerTest, InitiateBwu_Revert_OnDisconnect_WifiDirect) {
  SetSupportMultipleBwuMediums(true);
  OfflineFrame frame;
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  std::string bytes = parser::ForBwuWifiDirectPathAvailable(
      /*ssid=*/"", /*password=*/"", /*port=*/2143,
      /*frequency=*/2412, /*supports_disabling_encryption=*/false,
      /*gateway=*/"123.234.23.1", /*device_name=*/"NC-WifiDirectTest",
      /*pin=*/"b592f7d3");
  frame.ParseFromString(bytes);

  ::nearby::connections::V1Frame* v1_frame = frame.mutable_v1();
  ::nearby::connections::BandwidthUpgradeNegotiationFrame* sub_frame =
      v1_frame->mutable_bandwidth_upgrade_negotiation();
  BandwidthUpgradeNegotiationFrame::UpgradePathInfo* upgrade_path_info =
      sub_frame->mutable_upgrade_path_info();
  upgrade_path_info->set_supports_client_introduction_ack(false);
  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);
  CountDownLatch latch(1);
  bwu_manager_->OnEndpointDisconnect(&client_, (std::string)kServiceIdA,
                                     std::string(kEndpointId1), latch,
                                     DisconnectionReason::LOCAL_DISCONNECTION);

  ASSERT_EQ(fake_wifi_direct_bwu_handler_->disconnect_calls().size(), 1u);
  EXPECT_EQ(kEndpointId1,
            fake_wifi_direct_bwu_handler_->disconnect_calls()[0].endpoint_id);
  // This is called by the RESPONDER--call RevertInitiatorState only when
  // BWU Medium is Hotspot or WifiDirect.
  ASSERT_EQ(fake_wifi_direct_bwu_handler_->handle_revert_calls().size(), 1u);
  UnRegisterChannelForEndpoint(kEndpointId1);
}

TEST_F(BwuManagerTest, InitiateBwu_Revert_OnDisconnect_Hotspot) {
  SetSupportMultipleBwuMediums(true);

  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  ExceptionOr<OfflineFrame> hotspot_path_available_frame =
      parser::FromBytes(parser::ForBwuWifiHotspotPathAvailable(
          CreateWifiHotspotCredentials(),
          /*supports_disabling_encryption=*/false));
  ASSERT_TRUE(hotspot_path_available_frame.ok());
  OfflineFrame frame = hotspot_path_available_frame.result();
  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info = sub_frame->mutable_upgrade_path_info();
  upgrade_path_info->set_supports_client_introduction_ack(false);
  upgrade_path_info->set_supports_disabling_encryption(true);
  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);
  CountDownLatch latch(1);
  bwu_manager_->OnEndpointDisconnect(&client_, (std::string)kServiceIdA,
                                     std::string(kEndpointId1), latch,
                                     DisconnectionReason::LOCAL_DISCONNECTION);

  ASSERT_EQ(fake_wifi_hotspot_bwu_handler_->handle_revert_calls().size(), 1u);
  UnRegisterChannelForEndpoint(kEndpointId1);
}

TEST_F(BwuManagerTest, InitiateBwu_Revert_OnDisconnect_Wlan) {
  SetSupportMultipleBwuMediums(true);

  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  ExceptionOr<OfflineFrame> wlan_path_available_frame =
      parser::FromBytes(parser::ForBwuWifiLanPathAvailable(
          {ServiceAddress{.address = {'A', 'B', 'C', 'D'}, .port = 1234}}));
  OfflineFrame frame = wlan_path_available_frame.result();
  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info = sub_frame->mutable_upgrade_path_info();
  upgrade_path_info->set_supports_client_introduction_ack(false);

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);
  CountDownLatch latch(1);
  bwu_manager_->OnEndpointDisconnect(&client_, (std::string)kServiceIdA,
                                     std::string(kEndpointId1), latch,
                                     DisconnectionReason::LOCAL_DISCONNECTION);

  ASSERT_EQ(fake_wifi_lan_bwu_handler_->handle_revert_calls().size(), 0u);
  UnRegisterChannelForEndpoint(kEndpointId1);
}

TEST_F(BwuManagerTest, OnReceiveBwuEvent) {
  // TODO(b/235109434): Add more unit tests coverage for BWU module
}

TEST_F(BwuManagerTest, OnProcessBwuEvent) {
  // TODO(b/235109434): Add more unit tests coverage for BWU module
}

TEST_F(BwuManagerTest, BlockBwuFrameBeforeAccept) {
  auto channel = std::make_unique<FakeEndpointChannel>(
      Medium::BLUETOOTH, std::string(kServiceIdA));
  ecm_.RegisterChannelForEndpoint(&client_, std::string(kEndpointId2),
                                  std::move(channel));

  ExceptionOr<OfflineFrame> hotspot_path_available_frame2 =
      parser::FromBytes(parser::ForBwuWifiHotspotPathAvailable(
          CreateWifiHotspotCredentials(),
          /*supports_disabling_encryption=*/true));
  OfflineFrame frame2 = hotspot_path_available_frame2.result();
  frame2.set_version(OfflineFrame::V1);
  auto* v1_frame2 = frame2.mutable_v1();
  auto* sub_frame2 = v1_frame2->mutable_bandwidth_upgrade_negotiation();
  sub_frame2->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info2 = sub_frame2->mutable_upgrade_path_info();

  upgrade_path_info2->set_supports_client_introduction_ack(false);
  upgrade_path_info2->set_supports_disabling_encryption(true);
  bwu_manager_->OnIncomingFrame(frame2, std::string(kEndpointId2), &client_,
                                Medium::BLUETOOTH);
  CountDownLatch latch2(1);
  // The BWU frame should be drop, so the inProgressUpgrades should be empty.
  ASSERT_EQ(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId2)), false);
  UnRegisterChannelForEndpoint(kEndpointId2);
}

TEST_F(BwuManagerTest, BlockBwuFrameFromAdvertiser) {
  ExceptionOr<OfflineFrame> hotspot_path_available_frame =
      parser::FromBytes(parser::ForBwuWifiHotspotPathAvailable(
          CreateWifiHotspotCredentials(),
          /*supports_disabling_encryption=*/true));
  OfflineFrame frame = hotspot_path_available_frame.result();
  frame.set_version(OfflineFrame::V1);
  auto* v1_frame = frame.mutable_v1();
  auto* sub_frame = v1_frame->mutable_bandwidth_upgrade_negotiation();
  sub_frame->set_event_type(
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  auto* upgrade_path_info = sub_frame->mutable_upgrade_path_info();
  upgrade_path_info->set_supports_client_introduction_ack(false);
  upgrade_path_info->set_supports_disabling_encryption(true);

  ConnectionResponseInfo response_info{
      .remote_endpoint_info = ByteArray{"endpoint_name"},
      .authentication_token = "auth_token",
      .raw_authentication_token = ByteArray{"auth_token"},
      .is_incoming_connection = true,
  };
  ConnectionOptions connection_options;

  auto channel = std::make_unique<FakeEndpointChannel>(
      Medium::BLUETOOTH, std::string(kServiceIdA));
  ecm_.RegisterChannelForEndpoint(&client_, std::string(kEndpointId2),
                                  std::move(channel));

  client_.OnConnectionInitiated(std::string(kEndpointId2), response_info,
                                connection_options, {}, "token");
  client_.LocalEndpointAcceptedConnection(std::string(kEndpointId2), {});
  client_.RemoteEndpointAcceptedConnection(std::string(kEndpointId2));
  EXPECT_TRUE(client_.IsConnectionAccepted(std::string(kEndpointId2)));
  client_.OnConnectionAccepted(std::string(kEndpointId2));
  EXPECT_TRUE(client_.IsConnectedToEndpoint(std::string(kEndpointId2)));

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId2), &client_,
                                Medium::BLUETOOTH);
  CountDownLatch latch2(1);
  // The BWU frame should be drop, so the IsUpgradeOngoing should be empty.
  ASSERT_EQ(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId2)), false);
  UnRegisterChannelForEndpoint(kEndpointId2);
}

TEST_F(BwuManagerTest, ReceiveUnexpectedSafeToClose_NoCrash) {
  ExceptionOr<OfflineFrame> safe_to_close_frame =
      parser::FromBytes(parser::ForBwuSafeToClose());
  bwu_manager_->OnIncomingFrame(safe_to_close_frame.result(),
                                std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);
}

TEST_F(BwuManagerTest, ReceiveUnexpectedLastWrite_NoCrashOrWedge) {
  ExceptionOr<OfflineFrame> last_write_frame =
      parser::FromBytes(parser::ForBwuLastWrite());
  bwu_manager_->OnIncomingFrame(last_write_frame.result(),
                                std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);
}

TEST_F(BwuManagerTest, ReceiveEarlyLastWrite_Success) {
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  std::shared_ptr<EndpointChannel> shared_initial_channel =
      ecm_.GetChannelForEndpoint(kEndpointId1);

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WEB_RTC);
  ASSERT_TRUE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));

  ExceptionOr<OfflineFrame> last_write_frame =
      parser::FromBytes(parser::ForBwuLastWrite());
  bwu_manager_->OnIncomingFrame(last_write_frame.result(),
                                std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  FakeEndpointChannel* upgraded_channel =
      fake_web_rtc_bwu_handler_->NotifyBwuManagerOfIncomingConnection(
          /*initialize_call_index=*/0u, bwu_manager_.get());

  ExceptionOr<OfflineFrame> safe_to_close_frame =
      parser::FromBytes(parser::ForBwuSafeToClose());
  bwu_manager_->OnIncomingFrame(safe_to_close_frame.result(),
                                std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  auto old_channel =
      dynamic_cast<FakeEndpointChannel*>(shared_initial_channel.get());
  EXPECT_FALSE(upgraded_channel->IsPaused());
  EXPECT_TRUE(old_channel->is_closed());
  EXPECT_EQ(location::nearby::proto::connections::DisconnectionReason::UPGRADED,
            old_channel->disconnection_reason());
  UnRegisterChannelForEndpoint(kEndpointId1);
}

TEST_F(BwuManagerTest, ReceiveUnexpectedLastWriteBeforeUpgrade_NoWedge) {
  ExceptionOr<OfflineFrame> last_write_frame =
      parser::FromBytes(parser::ForBwuLastWrite());
  bwu_manager_->OnIncomingFrame(last_write_frame.result(),
                                std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  std::shared_ptr<EndpointChannel> shared_initial_channel =
      ecm_.GetChannelForEndpoint(kEndpointId1);

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WEB_RTC);

  FakeEndpointChannel* upgraded_channel =
      fake_web_rtc_bwu_handler_->NotifyBwuManagerOfIncomingConnection(
          /*initialize_call_index=*/0u, bwu_manager_.get());

  bwu_manager_->OnIncomingFrame(last_write_frame.result(),
                                std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  ExceptionOr<OfflineFrame> safe_to_close_frame =
      parser::FromBytes(parser::ForBwuSafeToClose());
  bwu_manager_->OnIncomingFrame(safe_to_close_frame.result(),
                                std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  auto old_channel =
      dynamic_cast<FakeEndpointChannel*>(shared_initial_channel.get());
  EXPECT_FALSE(upgraded_channel->IsPaused());
  EXPECT_TRUE(old_channel->is_closed());
  UnRegisterChannelForEndpoint(kEndpointId1);
}

TEST_F(BwuManagerTest, ProcessUpgradePathRequest_CanHost_True) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  ReinitBwuManager(BooleanMediumSelector{.wifi_direct = true});
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  location::nearby::connections::MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_direct_group_client(true);
  std::string bytes = parser::ForBwuPathRequest(
      Medium::WIFI_DIRECT, {Medium::WIFI_DIRECT}, remote_medium_role,
      /*supports_5_ghz=*/true);
  OfflineFrame frame;
  frame.ParseFromString(bytes);

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_EQ(fake_wifi_direct_bwu_handler_->handle_initialize_calls().size(),
            1u);

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that BwuManager initiates a Bandwidth Upgrade using Wi-Fi Aware when
// the local device can host as publisher and the Wi-Fi Aware medium is
// available.
TEST_F(BwuManagerTest,
       ProcessUpgradePathRequestCanHostWifiAwareAvailableSuccess) {
  MediumEnvironment::Instance().Start({.wifi_aware_enabled = true});
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::kEnableWifiAware,
      true);

  ReinitBwuManager(BooleanMediumSelector{.wifi_aware_r4 = true});

  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  location::nearby::connections::MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_aware_subscriber(true);
  std::string bytes = parser::ForBwuPathRequest(
      Medium::WIFI_AWARE_R4, {Medium::WIFI_AWARE_R4}, remote_medium_role,
      /*supports_5_ghz=*/true);
  OfflineFrame frame;
  frame.ParseFromString(bytes);

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_EQ(fake_wifi_aware_r4_bwu_handler_->handle_initialize_calls().size(),
            1u);

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that BwuManager does not initiate a Bandwidth Upgrade using Wi-Fi
// Aware when the Wi-Fi Aware medium is unavailable in MediumEnvironment, even
// if the feature flags and remote peer support it.
TEST_F(BwuManagerTest,
       ProcessUpgradePathRequestCanHostWifiAwareUnavailableFails) {
  MediumEnvironment::Instance().Stop();
  MediumEnvironment::Instance().Start({.wifi_aware_enabled = false});
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::kEnableWifiAware,
      true);

  ReinitBwuManager(BooleanMediumSelector{.wifi_aware_r4 = true});

  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  location::nearby::connections::MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_aware_subscriber(true);
  std::string bytes = parser::ForBwuPathRequest(
      Medium::WIFI_AWARE_R4, {Medium::WIFI_AWARE_R4}, remote_medium_role,
      /*supports_5_ghz=*/true);
  OfflineFrame frame;
  frame.ParseFromString(bytes);

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_EQ(fake_wifi_aware_r4_bwu_handler_->handle_initialize_calls().size(),
            0u);

  UnRegisterChannelForEndpoint(kEndpointId1);
}

TEST_F(BwuManagerTest, ProcessUpgradePathRequest_CanHost_False) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  ReinitBwuManager(BooleanMediumSelector{.wifi_direct = true});
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  location::nearby::connections::MediumRole remote_medium_role;
  std::string bytes = parser::ForBwuPathRequest(
      Medium::WIFI_DIRECT, {Medium::WIFI_DIRECT}, remote_medium_role,
      /*supports_5_ghz=*/true);
  OfflineFrame frame;
  frame.ParseFromString(bytes);

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_EQ(fake_wifi_direct_bwu_handler_->handle_initialize_calls().size(),
            0u);

  UnRegisterChannelForEndpoint(kEndpointId1);
}

TEST_F(BwuManagerTest, ProcessUpgradePathRequest_DynamicRoleSwitchDisabled) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      false);
  ReinitBwuManager(BooleanMediumSelector{.wifi_direct = true});
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  location::nearby::connections::MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_direct_group_client(true);
  std::string bytes = parser::ForBwuPathRequest(
      Medium::WIFI_DIRECT, {Medium::WIFI_DIRECT}, remote_medium_role,
      /*supports_5_ghz=*/true);
  OfflineFrame frame;
  frame.ParseFromString(bytes);

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_EQ(fake_wifi_direct_bwu_handler_->handle_initialize_calls().size(),
            0u);

  UnRegisterChannelForEndpoint(kEndpointId1);
}

TEST_F(BwuManagerTest, OnIncomingConnection_EndpointAliasesToLastEndpointId) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  ReinitBwuManager(BooleanMediumSelector{.web_rtc = true});
  CreateInitialEndpoint(&client_, kServiceIdA, "OldEndpoint",
                        Medium::BLUETOOTH);

  // Initiate upgrade for "OldEndpoint" (inserts into in_progress_upgrades_)
  bwu_manager_->InitiateBwuForEndpoint(&client_, "OldEndpoint",
                                       Medium::WEB_RTC);

  // Now simulate incoming upgraded connection. Set introduction read output:
  // - endpoint_id = "NewEndpoint"
  // - last_endpoint_id = "OldEndpoint"
  auto upgraded_channel = std::make_unique<FakeEndpointChannel>(
      Medium::WEB_RTC, std::string(kServiceIdA));
  FakeEndpointChannel* upgraded_channel_raw = upgraded_channel.get();

  std::string intro_frame = parser::ForBwuIntroduction(
      "NewEndpoint", "OldEndpoint", /*supports_disabling_encryption=*/false);
  upgraded_channel->set_read_output(
      ExceptionOr<ByteArray>(ByteArray(intro_frame)));

  auto connection = std::make_unique<BwuHandler::IncomingSocketConnection>(
      std::move(upgraded_channel));

  bwu_manager_->InvokeOnIncomingConnectionForTesting(&client_,
                                                     std::move(connection));

  EXPECT_NE(upgraded_channel_raw->GetLastWriteTimestamp(),
            absl::InfinitePast());

  UnRegisterChannelForEndpoint("OldEndpoint");
}

class BwuManagerTestParam : public BwuManagerTest,
                            public ::testing::WithParamInterface<bool> {
 protected:
  BwuManagerTestParam() { SetSupportMultipleBwuMediums(GetParam()); }
};

TEST_P(BwuManagerTestParam, InitiateBwu_Success) {
  // Create the initial device-to-device Bluetooth connection.
  FakeEndpointChannel* initial_channel = CreateInitialEndpoint(
      &client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  // Initiate BWU, and send BANDWIDTH_UPGRADE_NEGOTIATION.UPGRADE_PATH_AVAILABLE
  // to the Responder over the initial Bluetooth channel.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WEB_RTC);

  // The appropriate upgrade medium handler is informed of the BWU initiation.
  ASSERT_EQ(1u, fake_web_rtc_bwu_handler_->handle_initialize_calls().size());
  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(
      fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(fake_wifi_direct_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_EQ(WrapInitiatorUpgradeServiceId(kServiceIdA),
            fake_web_rtc_bwu_handler_->handle_initialize_calls()[0].service_id);
  EXPECT_EQ(
      kEndpointId1,
      fake_web_rtc_bwu_handler_->handle_initialize_calls()[0].endpoint_id);

  // Establish the incoming connection on the new medium. Verify that the
  // upgrade channel replaces the initial channel.
  std::shared_ptr<EndpointChannel> shared_initial_channel =
      ecm_.GetChannelForEndpoint(kEndpointId1);
  EXPECT_EQ(initial_channel, shared_initial_channel.get());
  FakeEndpointChannel* upgraded_channel =
      fake_web_rtc_bwu_handler_->NotifyBwuManagerOfIncomingConnection(
          /*initialize_call_index=*/0u, bwu_manager_.get());
  EXPECT_EQ(upgraded_channel, ecm_.GetChannelForEndpoint(kEndpointId1).get());

  // Confirm that upgrade channel is paused until initial channel is shut down.
  EXPECT_TRUE(upgraded_channel->IsPaused());
  EXPECT_FALSE(initial_channel->is_closed());

  // Receive BANDWIDTH_UPGRADE_NEGOTIATION.LAST_WRITE_TO_PRIOR_CHANNEL and then
  // BANDWIDTH_UPGRADE_NEGOTIATION.SAFE_TO_CLOSE_PRIOR_CHANNEL from the
  // Responder device to trigger the shutdown of the initial Bluetooth channel.
  ExceptionOr<OfflineFrame> last_write_frame =
      parser::FromBytes(parser::ForBwuLastWrite());
  bwu_manager_->OnIncomingFrame(last_write_frame.result(),
                                std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);
  ExceptionOr<OfflineFrame> safe_to_close_frame =
      parser::FromBytes(parser::ForBwuSafeToClose());
  bwu_manager_->OnIncomingFrame(safe_to_close_frame.result(),
                                std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  // Confirm that upgrade channel is resumed after initial channel is shut down.
  // Note: If we didn't grab the shared initial channel pointer above, this
  // channel would have already been destroyed.
  auto old_channel =
      dynamic_cast<FakeEndpointChannel*>(shared_initial_channel.get());
  EXPECT_FALSE(upgraded_channel->IsPaused());
  EXPECT_TRUE(old_channel->is_closed());
  EXPECT_EQ(location::nearby::proto::connections::DisconnectionReason::UPGRADED,
            old_channel->disconnection_reason());
  UnRegisterChannelForEndpoint(kEndpointId1);
}

TEST_P(BwuManagerTestParam,
       InitiateBwu_Error_DontUpgradeIfAlreadyConenctedOverTheRequestedMedium) {
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, /*initial_medium=*/Medium::BLUETOOTH,
                       /*upgrade_medium=*/Medium::WEB_RTC);
  EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_initialize_calls().size());

  // Ignore request to upgrade to WebRTC if we're already connected.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WEB_RTC);
  EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_initialize_calls().size());
  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that a BWU request is ignored if trying to upgrade from WLAN to
// Hotspot.
TEST_P(BwuManagerTestParam, InitiateBwu_IgnoresUpgradeFromWLANToHotspot) {
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::WIFI_LAN);

  // Ignore request to upgrade to WebRTC if we're already connected.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WIFI_HOTSPOT);
  EXPECT_TRUE(
      fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().empty());
  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that a BWU request is ignored if there is no initial medium
// connection.
TEST_P(BwuManagerTestParam, InitiateBwu_IgnoresWhenNoInitialMedium) {
  // Try to upgrade to a Medium without an initial Medium.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WIFI_HOTSPOT);

  // Make sure none of the other medium handlers are called.
  EXPECT_TRUE(fake_web_rtc_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(
      fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(fake_wifi_direct_bwu_handler_->handle_initialize_calls().empty());
}

// Verifies that a second upgrade request is ignored if an upgrade is already in
// progress.
TEST_P(BwuManagerTestParam, InitiateBwu_Error_BwuAlreadyInProgress) {
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  // Arrange: start an upgrade.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WEB_RTC);

  // Act: Try to upgrade an endpoint that already has an upgrade in progress.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WIFI_LAN);

  // Assert: should just early return with no action.
  EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_initialize_calls().size());
  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(
      fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(fake_wifi_direct_bwu_handler_->handle_initialize_calls().empty());
  UnRegisterChannelForEndpoint(kEndpointId1);
}

TEST_P(BwuManagerTestParam,
       InitiateBwu_Error_FailedToWriteUpgradePathAvailableFrame) {
  // Create the initial device-to-device Bluetooth connection.
  FakeEndpointChannel* initial_channel = CreateInitialEndpoint(
      &client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  // Make the initial endpoint channel fail when writing the
  // UPGRADE_PATH_AVAILABLE frame.
  initial_channel->set_write_output(Exception{Exception::kIo});

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WEB_RTC);

  // After we notify the WebRTC handler, we try to write the
  // UPGRADE_PATH_AVAILABLE frame, but fail by just early returning.
  EXPECT_EQ(1u, fake_web_rtc_bwu_handler_->handle_initialize_calls().size());

  // However, we do not record an in-progress attempt. So, if we see an incoming
  // connection over WebRTC, we ignore it. In other words, the initial BLUETOOTH
  // channel is still used.
  EXPECT_EQ(initial_channel, ecm_.GetChannelForEndpoint(kEndpointId1).get());
  FakeEndpointChannel* upgraded_channel =
      fake_web_rtc_bwu_handler_->NotifyBwuManagerOfIncomingConnection(
          /*initialize_call_index=*/0u, bwu_manager_.get());
  EXPECT_NE(upgraded_channel, ecm_.GetChannelForEndpoint(kEndpointId1).get());
  EXPECT_EQ(initial_channel, ecm_.GetChannelForEndpoint(kEndpointId1).get());
  UnRegisterChannelForEndpoint(kEndpointId1);
}

INSTANTIATE_TEST_SUITE_P(BwuManagerTestParam, BwuManagerTestParam,
                         testing::Bool());

// Creates a BwuManager with `config` and a fake WebRTC handler, so that no
// real medium handlers are created.
std::unique_ptr<BwuManager> CreateBwuManagerWithConfig(
    Mediums& mediums, EndpointManager& em, EndpointChannelManager& ecm,
    const BwuManager::Config& config) {
  absl::flat_hash_map<Medium, std::unique_ptr<BwuHandler>> handlers;
  handlers.emplace(Medium::WEB_RTC,
                   std::make_unique<FakeBwuHandler>(Medium::WEB_RTC));
  return std::make_unique<BwuManager>(mediums, em, ecm, std::move(handlers),
                                      config);
}

const std::vector<Medium>& AllAdvertisedMediums() {
  static const auto* const kMediums = new std::vector<Medium>{
      Medium::AWDL,        Medium::WIFI_LAN,     Medium::WIFI_AWARE_R4,
      Medium::WIFI_DIRECT, Medium::WIFI_HOTSPOT, Medium::WEB_RTC,
      Medium::BLUETOOTH,   Medium::BLE};
  return *kMediums;
}

TEST_F(BwuManagerBaseTest,
       StripOutDisallowedUpgradeMediums_WifiLanAndHotspotDisallowed_Removed) {
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  Mediums mediums;
  BwuManager::Config config;
  // `web_rtc` alone doesn't defeat `All(false)` (it is OR'ed with
  // `web_rtc_no_cellular`), which would make BwuManager replace this config
  // with flag defaults, so also allow `wifi_direct`.
  config.allow_upgrade_to.web_rtc = true;
  config.allow_upgrade_to.wifi_direct = true;
  config.allow_upgrade_to.wifi_lan = false;
  config.allow_upgrade_to.wifi_hotspot = false;
  auto bwu_manager = CreateBwuManagerWithConfig(mediums, em, ecm, config);

  EXPECT_THAT(
      bwu_manager->StripOutDisallowedUpgradeMediums(AllAdvertisedMediums()),
      testing::ElementsAre(Medium::AWDL, Medium::WIFI_AWARE_R4,
                           Medium::WIFI_DIRECT, Medium::WEB_RTC,
                           Medium::BLUETOOTH, Medium::BLE));
}

TEST_F(BwuManagerBaseTest,
       StripOutDisallowedUpgradeMediums_WifiLanAndHotspotAllowed_Kept) {
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  Mediums mediums;
  BwuManager::Config config;
  config.allow_upgrade_to.web_rtc = true;
  config.allow_upgrade_to.wifi_lan = true;
  config.allow_upgrade_to.wifi_hotspot = true;
  auto bwu_manager = CreateBwuManagerWithConfig(mediums, em, ecm, config);

  EXPECT_EQ(
      bwu_manager->StripOutDisallowedUpgradeMediums(AllAdvertisedMediums()),
      AllAdvertisedMediums());
}

TEST_F(BwuManagerBaseTest,
       StripOutDisallowedUpgradeMediums_DefaultConfig_FollowsFlagsDisabled) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::kEnableWifiLanUpgrade,
      false);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableWifiHotspotClient,
      false);
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  Mediums mediums;
  // An all-false config makes BwuManager fill in the defaults from flags.
  auto bwu_manager =
      CreateBwuManagerWithConfig(mediums, em, ecm, BwuManager::Config());

  EXPECT_THAT(bwu_manager->StripOutDisallowedUpgradeMediums(
                  {Medium::WIFI_LAN, Medium::WIFI_AWARE_R4,
                   Medium::WIFI_HOTSPOT, Medium::BLE}),
              testing::ElementsAre(Medium::WIFI_AWARE_R4, Medium::BLE));
}

TEST_F(BwuManagerBaseTest,
       StripOutDisallowedUpgradeMediums_DefaultConfig_FollowsFlagsEnabled) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::kEnableWifiLanUpgrade,
      true);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableWifiHotspotClient,
      true);
  EndpointChannelManager ecm;
  EndpointManager em(&ecm);
  Mediums mediums;
  auto bwu_manager =
      CreateBwuManagerWithConfig(mediums, em, ecm, BwuManager::Config());

  EXPECT_THAT(bwu_manager->StripOutDisallowedUpgradeMediums(
                  {Medium::WIFI_LAN, Medium::WIFI_AWARE_R4,
                   Medium::WIFI_HOTSPOT, Medium::BLE}),
              testing::ElementsAre(Medium::WIFI_LAN, Medium::WIFI_AWARE_R4,
                                   Medium::WIFI_HOTSPOT, Medium::BLE));
}

// Verifies that auto-selecting an upgrade medium (UNKNOWN_MEDIUM) on an Apple
// device strips out WIFI_DIRECT, retains WIFI_HOTSPOT when the remote peer
// supports hosting a hotspot, and sends an UPGRADE_PATH_REQUEST frame with the
// local medium role's wifi_hotspot_client capability set.
TEST_F(BwuManagerTest,
       InitiateBwuAutoSelectMediumAppleRoleSwitchSendsUpgradePathRequest) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  const BooleanMediumSelector allowed{.wifi_hotspot = true,
                                      .wifi_direct = true};
  ReinitBwuManager(allowed);

  client_.SetLocalOsType(OsInfo::APPLE);
  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_hotspot_host(true);
  FakeEndpointChannel* channel_ptr =
      CreateConnectedEndpoint(kEndpointId1, remote_medium_role, allowed);

  // Call InitiateBwuForEndpoint without specifying a medium (UNKNOWN_MEDIUM).
  // ChooseBestUpgradeMedium should strip out WIFI_DIRECT on Apple, keep
  // WIFI_HOTSPOT via CanSwitchRoleForMedium, and send UPGRADE_PATH_REQUEST.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1));

  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_TRUE(fake_wifi_direct_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(
      fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().empty());
  ASSERT_EQ(channel_ptr->GetWrittenFrames().size(), 1u);
  ExceptionOr<OfflineFrame> written_frame =
      parser::FromBytes(channel_ptr->GetWrittenFrames()[0]);
  ASSERT_TRUE(written_frame.ok());
  const auto& bwu_frame =
      written_frame.result().v1().bandwidth_upgrade_negotiation();
  EXPECT_EQ(bwu_frame.event_type(),
            BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_REQUEST);
  EXPECT_TRUE(bwu_frame.upgrade_path_info()
                  .upgrade_path_request()
                  .medium_meta_data()
                  .medium_role()
                  .support_wifi_hotspot_client());

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that on an Apple device when the remote peer is also STA-only and
// cannot host a Wi-Fi Hotspot, WIFI_DIRECT and WIFI_HOTSPOT are stripped out
// during initial auto-selection (UNKNOWN_MEDIUM) without starting an upgrade.
TEST_F(BwuManagerTest,
       InitiateBwuAutoSelectMediumAppleNoRoleSwitchStripsDirectAndHotspot) {
  SetSupportMultipleBwuMediums(true);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  const BooleanMediumSelector allowed{.wifi_hotspot = true,
                                      .wifi_direct = true};
  ReinitBwuManager(allowed);

  client_.SetLocalOsType(OsInfo::APPLE);
  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_hotspot_host(false);
  remote_medium_role.set_support_wifi_hotspot_client(true);
  FakeEndpointChannel* channel_ptr =
      CreateConnectedEndpoint(kEndpointId1, remote_medium_role, allowed);

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1));

  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_TRUE(fake_wifi_direct_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(
      fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(channel_ptr->GetWrittenFrames().empty());

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that on an Apple device when WIFI_LAN upgrade fails and the remote
// peer cannot host a Wi-Fi Hotspot, TryNextBestUpgradeMediums strips out
// WIFI_HOTSPOT rather than attempting to start a local SoftAP.
TEST_F(BwuManagerTest,
       ProcessUpgradeFailureEventAppleNoRoleSwitchSkipsHotspotFallback) {
  SetSupportMultipleBwuMediums(true);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  const BooleanMediumSelector allowed{.wifi_lan = true, .wifi_hotspot = true};
  ReinitBwuManager(allowed);

  client_.SetLocalOsType(OsInfo::APPLE);
  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_hotspot_host(false);
  remote_medium_role.set_support_wifi_hotspot_client(true);
  CreateConnectedEndpoint(kEndpointId1, remote_medium_role, allowed);

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1));
  ASSERT_EQ(fake_wifi_lan_bwu_handler_->handle_initialize_calls().size(), 1u);

  BandwidthUpgradeNegotiationFrame::UpgradePathInfo failure_info;
  failure_info.set_medium(
      BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WIFI_LAN);
  ExceptionOr<OfflineFrame> upgrade_failure =
      parser::FromBytes(parser::ForBwuFailure(failure_info));
  ASSERT_TRUE(upgrade_failure.ok());
  bwu_manager_->OnIncomingFrame(upgrade_failure.result(),
                                std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_EQ(fake_wifi_lan_bwu_handler_->handle_revert_calls().size(), 1u);
  EXPECT_TRUE(
      fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that auto-selecting an upgrade medium (UNKNOWN_MEDIUM) on Windows
// when no P2P connection is active selects WIFI_DIRECT and delegates Group
// Owner hosting to an Android peer via UPGRADE_PATH_REQUEST.
TEST_F(BwuManagerTest,
       InitiateBwuAutoSelectMediumWindowsAndroidRoleSwitchSendsRequest) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  const BooleanMediumSelector allowed{.wifi_direct = true};
  ReinitBwuManager(allowed);

  client_.SetLocalOsType(OsInfo::WINDOWS);
  ASSERT_FALSE(client_.IsUsingP2pMedium());

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_direct_group_owner(true);
  FakeEndpointChannel* channel_ptr = CreateConnectedEndpoint(
      kEndpointId1, remote_medium_role, allowed, OsInfo::ANDROID);

  // Auto-select medium (UNKNOWN_MEDIUM) on Windows -> Android with GO support.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1));

  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_TRUE(fake_wifi_direct_bwu_handler_->handle_initialize_calls().empty());
  ASSERT_EQ(channel_ptr->GetWrittenFrames().size(), 1u);
  ExceptionOr<OfflineFrame> written_frame =
      parser::FromBytes(channel_ptr->GetWrittenFrames()[0]);
  ASSERT_TRUE(written_frame.ok());
  const auto& bwu_frame =
      written_frame.result().v1().bandwidth_upgrade_negotiation();
  EXPECT_EQ(bwu_frame.event_type(),
            BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_REQUEST);
  EXPECT_EQ(bwu_frame.upgrade_path_info().medium(),
            BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WIFI_DIRECT);
  EXPECT_TRUE(bwu_frame.upgrade_path_info()
                  .upgrade_path_request()
                  .medium_meta_data()
                  .medium_role()
                  .support_wifi_direct_group_owner());
  EXPECT_TRUE(bwu_frame.upgrade_path_info()
                  .upgrade_path_request()
                  .medium_meta_data()
                  .medium_role()
                  .support_wifi_direct_group_client());

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that on Windows when an existing endpoint is already connected over
// WIFI_DIRECT (IsUsingP2pMedium is true), upgrading a second Android endpoint
// reuses the local Group Owner instead of switching roles to Group Client.
TEST_F(BwuManagerTest,
       InitiateBwuAutoSelectMediumWindowsAndroidSecondEndpointReusesLocalGo) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  const BooleanMediumSelector allowed{.wifi_direct = true};
  ReinitBwuManager(allowed);

  client_.SetLocalOsType(OsInfo::WINDOWS);
  client_.OnConnectionInitiated(
      std::string(kEndpointId2),
      {.remote_endpoint_info = ByteArray("existing p2p endpoint")},
      ConnectionOptions{.auto_upgrade_bandwidth = false}, {}, "");
  client_.OnConnectionAccepted(std::string(kEndpointId2));
  client_.OnBandwidthChanged(std::string(kEndpointId2), Medium::WIFI_DIRECT);
  ASSERT_TRUE(client_.IsUsingP2pMedium());

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_direct_group_owner(true);
  FakeEndpointChannel* channel_ptr = CreateConnectedEndpoint(
      kEndpointId1, remote_medium_role, allowed, OsInfo::ANDROID);

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1));

  EXPECT_TRUE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_EQ(fake_wifi_direct_bwu_handler_->handle_initialize_calls().size(),
            1u);
  ASSERT_EQ(channel_ptr->GetWrittenFrames().size(), 1u);
  ExceptionOr<OfflineFrame> written_frame =
      parser::FromBytes(channel_ptr->GetWrittenFrames()[0]);
  ASSERT_TRUE(written_frame.ok());
  const auto& bwu_frame =
      written_frame.result().v1().bandwidth_upgrade_negotiation();
  EXPECT_EQ(bwu_frame.event_type(),
            BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  EXPECT_EQ(bwu_frame.upgrade_path_info().medium(),
            BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WIFI_DIRECT);

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that on Android when a first endpoint is already connected over
// WIFI_HOTSPOT (IsUsingP2pMedium is true), auto-selecting an upgrade medium for
// a second endpoint retains WIFI_HOTSPOT and reuses the local SoftAP.
TEST_F(BwuManagerTest,
       InitiateBwuAutoSelectMediumAndroidSecondEndpointReusesHotspot) {
  SetSupportMultipleBwuMediums(false);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  const BooleanMediumSelector allowed{.wifi_hotspot = true};
  ReinitBwuManager(allowed);

  client_.SetLocalOsType(OsInfo::ANDROID);
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId2, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId2, Medium::BLUETOOTH, Medium::WIFI_HOTSPOT);
  ASSERT_TRUE(client_.IsUsingP2pMedium());

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_hotspot_client(true);
  FakeEndpointChannel* channel_ptr = CreateConnectedEndpoint(
      kEndpointId1, remote_medium_role, allowed, OsInfo::ANDROID);

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1));

  EXPECT_TRUE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_EQ(fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().size(),
            2u);
  ASSERT_EQ(channel_ptr->GetWrittenFrames().size(), 1u);
  ExceptionOr<OfflineFrame> written_frame =
      parser::FromBytes(channel_ptr->GetWrittenFrames()[0]);
  ASSERT_TRUE(written_frame.ok());
  EXPECT_EQ(
      written_frame.result().v1().bandwidth_upgrade_negotiation().event_type(),
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);

  UnRegisterChannelForEndpoint(kEndpointId1);
  UnRegisterChannelForEndpoint(kEndpointId2);
}

// Verifies that an Apple Advertiser (incoming connection) accepts and processes
// an incoming WIFI_HOTSPOT UPGRADE_PATH_AVAILABLE frame when dynamic role
// switching is enabled and the remote peer hosts the hotspot.
TEST_F(BwuManagerTest,
       ProcessBwuPathAvailableEventAppleAdvertiserWithRoleSwitchSucceeds) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  const BooleanMediumSelector allowed{.wifi_hotspot = true};
  ReinitBwuManager(allowed);

  client_.SetLocalOsType(OsInfo::APPLE);
  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_hotspot_host(true);
  CreateConnectedEndpoint(kEndpointId1, remote_medium_role, allowed,
                          /*remote_os_type=*/std::nullopt,
                          /*is_incoming_connection=*/true);
  ASSERT_TRUE(client_.IsConnectedToEndpoint(std::string(kEndpointId1)));

  ExceptionOr<OfflineFrame> hotspot_path_available_frame =
      parser::FromBytes(parser::ForBwuWifiHotspotPathAvailable(
          CreateWifiHotspotCredentials(),
          /*supports_disabling_encryption=*/true));
  ASSERT_TRUE(hotspot_path_available_frame.ok());
  OfflineFrame frame = hotspot_path_available_frame.result();
  frame.mutable_v1()
      ->mutable_bandwidth_upgrade_negotiation()
      ->mutable_upgrade_path_info()
      ->set_supports_client_introduction_ack(false);

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_EQ(fake_wifi_hotspot_bwu_handler_->create_calls().size(), 1u);
  EXPECT_TRUE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that when an Apple Advertiser accepts an incoming WIFI_HOTSPOT
// UPGRADE_PATH_AVAILABLE frame via role switch but
// CreateUpgradedEndpointChannel returns nullptr, BwuManager sends an
// UPGRADE_FAILURE frame and keeps the base BLUETOOTH channel open.
TEST_F(BwuManagerTest,
       ProcessBwuPathAvailableAppleAdvertiserFailSendsUpgradeFail) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  // Configure FakeBwuHandler for WIFI_HOTSPOT to return nullptr from
  // CreateUpgradedEndpointChannel so ProcessBwuPathAvailableEventInternal
  // sends UPGRADE_FAILURE.
  ReinitBwuManager(
      BooleanMediumSelector{.wifi_hotspot = true, .wifi_direct = true});
  fake_wifi_hotspot_bwu_handler_->set_return_null_channel(true);

  client_.SetLocalOsType(OsInfo::APPLE);
  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_hotspot_host(true);
  FakeEndpointChannel* channel_ptr = CreateConnectedEndpoint(
      kEndpointId1, remote_medium_role,
      BooleanMediumSelector{.wifi_hotspot = true},
      /*remote_os_type=*/std::nullopt, /*is_incoming_connection=*/true);

  ExceptionOr<OfflineFrame> hotspot_path_available_frame =
      parser::FromBytes(parser::ForBwuWifiHotspotPathAvailable(
          CreateWifiHotspotCredentials(),
          /*supports_disabling_encryption=*/true));
  ASSERT_TRUE(hotspot_path_available_frame.ok());
  OfflineFrame frame = hotspot_path_available_frame.result();
  frame.mutable_v1()
      ->mutable_bandwidth_upgrade_negotiation()
      ->mutable_upgrade_path_info()
      ->set_supports_client_introduction_ack(false);

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_FALSE(channel_ptr->is_closed());
  ASSERT_EQ(channel_ptr->GetWrittenFrames().size(), 1u);
  ExceptionOr<OfflineFrame> written_frame =
      parser::FromBytes(channel_ptr->GetWrittenFrames()[0]);
  ASSERT_TRUE(written_frame.ok());
  EXPECT_EQ(
      written_frame.result().v1().bandwidth_upgrade_negotiation().event_type(),
      BandwidthUpgradeNegotiationFrame::UPGRADE_FAILURE);

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that ProcessUpgradePathRequest prioritizes WIFI_DIRECT ahead of
// WIFI_HOTSPOT when the requesting peer lists WIFI_HOTSPOT before WIFI_DIRECT
// and supports both client roles.
TEST_F(BwuManagerTest,
       ProcessUpgradePathRequestPrioritizesWifiDirectOverWifiHotspot) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  ReinitBwuManager(
      BooleanMediumSelector{.wifi_hotspot = true, .wifi_direct = true});

  client_.SetLocalOsType(OsInfo::ANDROID);
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_direct_group_client(true);
  remote_medium_role.set_support_wifi_hotspot_client(true);
  std::string bytes = parser::ForBwuPathRequest(
      Medium::WIFI_DIRECT, {Medium::WIFI_HOTSPOT, Medium::WIFI_DIRECT},
      remote_medium_role, /*supports_5_ghz=*/true);
  OfflineFrame frame;
  ASSERT_TRUE(frame.ParseFromString(bytes));

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_EQ(fake_wifi_direct_bwu_handler_->handle_initialize_calls().size(),
            1u);
  EXPECT_TRUE(
      fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().empty());

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that ProcessUpgradePathRequest preserves the canonical medium
// priority order (WIFI_AWARE_R4 ahead of WIFI_DIRECT) when the requesting peer
// supports both subscriber/client roles.
TEST_F(BwuManagerTest,
       ProcessUpgradePathRequestPrioritizesWifiAwareR4OverWifiDirect) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  ReinitBwuManager(
      BooleanMediumSelector{.wifi_direct = true, .wifi_aware_r4 = true});

  client_.SetLocalOsType(OsInfo::ANDROID);
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_aware_subscriber(true);
  remote_medium_role.set_support_wifi_direct_group_client(true);
  std::string bytes = parser::ForBwuPathRequest(
      Medium::WIFI_AWARE_R4, {Medium::WIFI_AWARE_R4, Medium::WIFI_DIRECT},
      remote_medium_role, /*supports_5_ghz=*/true);
  OfflineFrame frame;
  ASSERT_TRUE(frame.ParseFromString(bytes));

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_EQ(fake_wifi_aware_r4_bwu_handler_->handle_initialize_calls().size(),
            1u);
  EXPECT_TRUE(fake_wifi_direct_bwu_handler_->handle_initialize_calls().empty());

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that ProcessUpgradePathRequest fails the upgrade via
// ProcessUpgradeFailureEvent without initiating an unrequested medium when
// CanHost is true for one medium (WIFI_DIRECT) but request.mediums() only
// requests an unavailable medium (WIFI_HOTSPOT).
TEST_F(BwuManagerTest,
       ProcessUpgradePathRequestRequestedMediumUnavailableFailsUpgrade) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  ReinitBwuManager(BooleanMediumSelector{.wifi_direct = true});

  client_.SetLocalOsType(OsInfo::ANDROID);
  FakeEndpointChannel* channel = CreateInitialEndpoint(
      &client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  // CanHost is true because WIFI_DIRECT GO is available locally and
  // remote_medium_role supports wifi_direct_group_client, but the peer's
  // request.mediums() only lists WIFI_HOTSPOT (which has no handler).
  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_direct_group_client(true);
  remote_medium_role.set_support_wifi_hotspot_client(true);
  std::string bytes = parser::ForBwuPathRequest(
      Medium::WIFI_HOTSPOT, {Medium::WIFI_HOTSPOT}, remote_medium_role,
      /*supports_5_ghz=*/true);
  OfflineFrame frame;
  ASSERT_TRUE(frame.ParseFromString(bytes));

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_TRUE(fake_wifi_direct_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_TRUE(channel->GetWrittenFrames().empty());

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that ProcessUpgradePathRequest falls back to hosting WIFI_HOTSPOT
// instead of failing the upgrade when the peer requests [WIFI_DIRECT,
// WIFI_HOTSPOT] and supports both client roles, but local WIFI_DIRECT Group
// Owner is unavailable.
TEST_F(BwuManagerTest,
       ProcessUpgradePathRequestFallsBackToHotspotWhenWifiDirectGoUnavailable) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::kEnableWifiDirect,
      false);
  ReinitBwuManager(
      BooleanMediumSelector{.wifi_hotspot = true, .wifi_direct = true});
  ASSERT_FALSE(mediums_->GetWifiDirect().IsGOAvailable());
  ASSERT_TRUE(mediums_->GetWifiHotspot().IsAPAvailable());

  client_.SetLocalOsType(OsInfo::ANDROID);
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_direct_group_client(true);
  remote_medium_role.set_support_wifi_hotspot_client(true);
  std::string bytes = parser::ForBwuPathRequest(
      Medium::WIFI_DIRECT, {Medium::WIFI_DIRECT, Medium::WIFI_HOTSPOT},
      remote_medium_role, /*supports_5_ghz=*/true);
  OfflineFrame frame;
  ASSERT_TRUE(frame.ParseFromString(bytes));

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_TRUE(fake_wifi_direct_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_EQ(fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().size(),
            1u);
  EXPECT_TRUE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that when InitiateBwuForEndpoint delegates an initial WIFI_DIRECT
// upgrade via UPGRADE_PATH_REQUEST, the endpoint BWU medium remains unset so a
// subsequent UPGRADE_PATH_AVAILABLE frame for a fallback medium (WIFI_HOTSPOT)
// chosen by the remote host succeeds.
TEST_F(BwuManagerTest,
       InitiateBwuRoleSwitchRequestAcceptsFallbackMediumFromRemoteHost) {
  SetSupportMultipleBwuMediums(true);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  const BooleanMediumSelector allowed{.wifi_hotspot = true,
                                      .wifi_direct = true};
  ReinitBwuManager(allowed);

  client_.SetLocalOsType(OsInfo::WINDOWS);
  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_direct_group_owner(true);
  remote_medium_role.set_support_wifi_hotspot_host(true);
  FakeEndpointChannel* channel_ptr = CreateConnectedEndpoint(
      kEndpointId1, remote_medium_role, allowed, OsInfo::ANDROID,
      /*is_incoming_connection=*/true);

  // Initial auto-selection picks WIFI_DIRECT and sends UPGRADE_PATH_REQUEST.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1));

  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_TRUE(fake_wifi_direct_bwu_handler_->handle_initialize_calls().empty());
  ASSERT_EQ(channel_ptr->GetWrittenFrames().size(), 1u);
  ExceptionOr<OfflineFrame> request_frame =
      parser::FromBytes(channel_ptr->GetWrittenFrames()[0]);
  ASSERT_TRUE(request_frame.ok());
  EXPECT_EQ(
      request_frame.result().v1().bandwidth_upgrade_negotiation().event_type(),
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_REQUEST);

  // Remote host falls back to WIFI_HOTSPOT and replies with
  // UPGRADE_PATH_AVAILABLE for WIFI_HOTSPOT.
  OfflineFrame available_frame =
      CreateWifiHotspotPathAvailableFrameWithoutClientIntroAck();
  bwu_manager_->OnIncomingFrame(available_frame, std::string(kEndpointId1),
                                &client_, Medium::BLUETOOTH);

  EXPECT_EQ(fake_wifi_hotspot_bwu_handler_->create_calls().size(), 1u);
  EXPECT_TRUE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that auto-selecting an upgrade medium (UNKNOWN_MEDIUM) on an Apple
// advertiser against an Android peer that supports Wi-Fi Aware publishing
// delegates the upgrade via a single UPGRADE_PATH_REQUEST frame advertising
// local Wi-Fi Aware subscriber support.
TEST_F(BwuManagerTest,
       InitiateBwuAutoSelectMediumWifiAwareRoleSwitchSendsRequest) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  const BooleanMediumSelector allowed{.wifi_aware = true};
  ReinitBwuManager(allowed);

  client_.SetLocalOsType(OsInfo::APPLE);
  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_aware_publisher(true);
  remote_medium_role.set_support_wifi_aware_subscriber(false);
  FakeEndpointChannel* channel_ptr =
      CreateConnectedEndpoint(kEndpointId1, remote_medium_role, allowed,
                              OsInfo::ANDROID, /*is_incoming_connection=*/true);

  // Auto-select medium (UNKNOWN_MEDIUM) on Apple advertiser -> Android peer.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1));

  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_TRUE(
      fake_wifi_aware_r4_bwu_handler_->handle_initialize_calls().empty());
  ASSERT_EQ(channel_ptr->GetWrittenFrames().size(), 1u);
  ExceptionOr<OfflineFrame> written_frame =
      parser::FromBytes(channel_ptr->GetWrittenFrames()[0]);
  ASSERT_TRUE(written_frame.ok());
  const auto& bwu_frame =
      written_frame.result().v1().bandwidth_upgrade_negotiation();
  EXPECT_EQ(bwu_frame.event_type(),
            BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_REQUEST);
  EXPECT_EQ(bwu_frame.upgrade_path_info().medium(),
            BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WIFI_AWARE_R4);
  EXPECT_TRUE(bwu_frame.upgrade_path_info()
                  .upgrade_path_request()
                  .medium_meta_data()
                  .medium_role()
                  .support_wifi_aware_subscriber());
  EXPECT_FALSE(bwu_frame.upgrade_path_info()
                   .upgrade_path_request()
                   .medium_meta_data()
                   .medium_role()
                   .support_wifi_aware_publisher());

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that when local Wi-Fi Aware is unavailable (e.g. kEnableWifiAware is
// disabled or on macOS), StripOutUnavailableMediums strips out WIFI_AWARE_R4
// during auto-selection even if CanSwitchRoleForMedium would be true.
TEST_F(BwuManagerTest,
       InitiateBwuAutoSelectMediumWifiAwareUnavailableSkipsRequest) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::kEnableWifiAware,
      false);
  const BooleanMediumSelector allowed{.wifi_aware = true};
  ReinitBwuManager(allowed);

  client_.SetLocalOsType(OsInfo::APPLE);
  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_aware_publisher(true);
  FakeEndpointChannel* channel_ptr =
      CreateConnectedEndpoint(kEndpointId1, remote_medium_role, allowed,
                              OsInfo::ANDROID, /*is_incoming_connection=*/true);

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1));

  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_TRUE(
      fake_wifi_aware_r4_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(channel_ptr->GetWrittenFrames().empty());

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that ProcessUpgradePathRequest on an Android device with Wi-Fi Aware
// available satisfies CanHost for WIFI_AWARE_R4 and initiates publishing when
// the requesting peer advertises support_wifi_aware_subscriber.
TEST_F(BwuManagerTest,
       ProcessUpgradePathRequestSelectsWifiAwareR4WhenSupported) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  ReinitBwuManager(BooleanMediumSelector{.wifi_aware_r4 = true});

  client_.SetLocalOsType(OsInfo::ANDROID);
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_aware_subscriber(true);
  std::string bytes = parser::ForBwuPathRequest(
      Medium::WIFI_AWARE_R4, {Medium::WIFI_AWARE_R4}, remote_medium_role,
      /*supports_5_ghz=*/true);
  OfflineFrame frame;
  ASSERT_TRUE(frame.ParseFromString(bytes));

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_EQ(fake_wifi_aware_r4_bwu_handler_->handle_initialize_calls().size(),
            1u);
  EXPECT_TRUE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that ProcessUpgradePathRequest filters out non-role-switched mediums
// (WIFI_LAN) and unsupported role-switch mediums (WIFI_DIRECT) via CanHost so
// they do not preempt hosting WIFI_HOTSPOT.
TEST_F(BwuManagerTest,
       ProcessUpgradePathRequestFiltersNonRoleSwitchMediumsAndSelectsHotspot) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  ReinitBwuManager(BooleanMediumSelector{
      .wifi_lan = true, .wifi_hotspot = true, .wifi_direct = true});

  client_.SetLocalOsType(OsInfo::ANDROID);
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_direct_group_client(false);
  remote_medium_role.set_support_wifi_hotspot_client(true);
  std::string bytes = parser::ForBwuPathRequest(
      Medium::WIFI_HOTSPOT,
      {Medium::WIFI_LAN, Medium::WIFI_DIRECT, Medium::WIFI_HOTSPOT},
      remote_medium_role, /*supports_5_ghz=*/true);
  OfflineFrame frame;
  ASSERT_TRUE(frame.ParseFromString(bytes));

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(fake_wifi_direct_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_EQ(fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().size(),
            1u);

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that ProcessUpgradePathRequest on Windows hosts WIFI_DIRECT when the
// Android peer already delegated the host role via UPGRADE_PATH_REQUEST, even
// if the Android peer's initial ConnectionInfo also advertised Group Owner
// support.
TEST_F(BwuManagerTest,
       ProcessUpgradePathRequestWindowsHostsWhenAndroidPeerSupportsGo) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  const BooleanMediumSelector allowed{.wifi_direct = true};
  ReinitBwuManager(allowed);

  client_.SetLocalOsType(OsInfo::WINDOWS);
  MediumRole initial_remote_medium_role;
  initial_remote_medium_role.set_support_wifi_direct_group_owner(true);
  initial_remote_medium_role.set_support_wifi_direct_group_client(true);
  FakeEndpointChannel* channel_ptr = CreateConnectedEndpoint(
      kEndpointId1, initial_remote_medium_role, allowed, OsInfo::ANDROID);

  MediumRole request_medium_role;
  request_medium_role.set_support_wifi_direct_group_owner(true);
  request_medium_role.set_support_wifi_direct_group_client(true);
  std::string bytes = parser::ForBwuPathRequest(
      Medium::WIFI_DIRECT, {Medium::WIFI_DIRECT}, request_medium_role,
      /*supports_5_ghz=*/true);
  OfflineFrame frame;
  ASSERT_TRUE(frame.ParseFromString(bytes));

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_EQ(fake_wifi_direct_bwu_handler_->handle_initialize_calls().size(),
            1u);
  EXPECT_TRUE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  ASSERT_EQ(channel_ptr->GetWrittenFrames().size(), 1u);
  ExceptionOr<OfflineFrame> written_frame =
      parser::FromBytes(channel_ptr->GetWrittenFrames()[0]);
  ASSERT_TRUE(written_frame.ok());
  EXPECT_EQ(
      written_frame.result().v1().bandwidth_upgrade_negotiation().event_type(),
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that ProcessUpgradePathRequest selects and initializes WIFI_DIRECT
// even when the endpoint already completed an earlier bandwidth upgrade to
// WIFI_LAN (so GetBwuMediumForEndpoint is WIFI_LAN rather than UNKNOWN_MEDIUM).
TEST_F(BwuManagerTest,
       ProcessUpgradePathRequestUpgradesFromActiveWifiLanToWifiDirect) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  ReinitBwuManager(
      BooleanMediumSelector{.wifi_lan = true, .wifi_direct = true});

  client_.SetLocalOsType(OsInfo::ANDROID);
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId1, Medium::BLUETOOTH);
  FullyUpgradeEndpoint(kEndpointId1, Medium::BLUETOOTH, Medium::WIFI_LAN);
  ASSERT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_direct_group_client(true);
  std::string bytes = parser::ForBwuPathRequest(
      Medium::WIFI_DIRECT, {Medium::WIFI_LAN, Medium::WIFI_DIRECT},
      remote_medium_role, /*supports_5_ghz=*/true);
  OfflineFrame frame;
  ASSERT_TRUE(frame.ParseFromString(bytes));

  bwu_manager_->OnIncomingFrame(frame, std::string(kEndpointId1), &client_,
                                Medium::WIFI_LAN);

  EXPECT_EQ(fake_wifi_direct_bwu_handler_->handle_initialize_calls().size(),
            1u);
  EXPECT_TRUE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that when an Apple device connects to an Android device that can
// publish Wi-Fi Aware R4, ChooseBestUpgradeMedium correctly selects
// WIFI_AWARE_R4 and NeedToSwitchRole delegates hosting to the peer via
// UPGRADE_PATH_REQUEST.
TEST_F(BwuManagerTest,
       InitiateBwuAutoSelectMediumAppleAwareR4AndroidPeerSwitchesRole) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  ReinitBwuManager(BooleanMediumSelector{.wifi_aware_r4 = true});

  client_.SetLocalOsType(OsInfo::APPLE);

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_aware_publisher(true);
  FakeEndpointChannel* channel_ptr =
      CreateConnectedEndpoint(kEndpointId1, remote_medium_role,
                              BooleanMediumSelector{.wifi_aware_r4 = true},
                              OsInfo::ANDROID, /*is_incoming_connection=*/true);

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1));

  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_TRUE(
      fake_wifi_aware_r4_bwu_handler_->handle_initialize_calls().empty());
  ASSERT_EQ(channel_ptr->GetWrittenFrames().size(), 1u);
  ExceptionOr<OfflineFrame> written_frame =
      parser::FromBytes(channel_ptr->GetWrittenFrames()[0]);
  ASSERT_TRUE(written_frame.ok());
  const auto& bwu_frame =
      written_frame.result().v1().bandwidth_upgrade_negotiation();
  EXPECT_EQ(bwu_frame.event_type(),
            BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_REQUEST);
  EXPECT_EQ(bwu_frame.upgrade_path_info().medium(), Medium::WIFI_AWARE_R4);

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that on an Apple device when the remote peer is also Apple,
// ChooseBestUpgradeMedium selects WIFI_AWARE_R4 and hosts locally instead of
// stripping it out.
TEST_F(BwuManagerTest,
       InitiateBwuAutoSelectMediumAppleAwareR4ApplePeerHostsLocally) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  ReinitBwuManager(BooleanMediumSelector{.wifi_aware_r4 = true});

  client_.SetLocalOsType(OsInfo::APPLE);

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_aware_publisher(false);
  FakeEndpointChannel* channel_ptr =
      CreateConnectedEndpoint(kEndpointId1, remote_medium_role,
                              BooleanMediumSelector{.wifi_aware_r4 = true},
                              OsInfo::APPLE, /*is_incoming_connection=*/true);

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1));

  EXPECT_TRUE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_EQ(fake_wifi_aware_r4_bwu_handler_->handle_initialize_calls().size(),
            1u);
  ASSERT_EQ(channel_ptr->GetWrittenFrames().size(), 1u);
  ExceptionOr<OfflineFrame> written_frame =
      parser::FromBytes(channel_ptr->GetWrittenFrames()[0]);
  ASSERT_TRUE(written_frame.ok());
  const auto& bwu_frame =
      written_frame.result().v1().bandwidth_upgrade_negotiation();
  EXPECT_EQ(bwu_frame.event_type(),
            BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_AVAILABLE);
  EXPECT_EQ(bwu_frame.upgrade_path_info().medium(), Medium::WIFI_AWARE_R4);

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that on an Apple advertiser when the remote peer is Android and
// cannot publish Wi-Fi Aware R4, StripOutUnavailableMediums strips out
// WIFI_AWARE_R4 because Apple cannot publish to Android subscribers.
TEST_F(BwuManagerTest,
       InitiateBwuAutoSelectAppleAwareR4AndroidWithoutPublisherStripsOut) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  ReinitBwuManager(BooleanMediumSelector{.wifi_aware_r4 = true});

  client_.SetLocalOsType(OsInfo::APPLE);

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_aware_publisher(false);
  FakeEndpointChannel* channel_ptr =
      CreateConnectedEndpoint(kEndpointId1, remote_medium_role,
                              BooleanMediumSelector{.wifi_aware_r4 = true},
                              OsInfo::ANDROID, /*is_incoming_connection=*/true);

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1));

  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_TRUE(
      fake_wifi_aware_r4_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_TRUE(channel_ptr->GetWrittenFrames().empty());

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that when TryNextBestUpgradeMediums attempts a fallback medium
// (WIFI_HOTSPOT) that early-returns in InitiateBwuForEndpoint because another
// endpoint is connected over WIFI_LAN, the endpoint's BWU medium remains unset
// so a subsequent incoming WIFI_LAN UPGRADE_PATH_AVAILABLE frame succeeds.
TEST_F(BwuManagerTest,
       TryNextBestUpgradeMediumsEarlyReturnPreservesUnsetBwuMedium) {
  SetSupportMultipleBwuMediums(true);
  const BooleanMediumSelector allowed{.wifi_lan = true, .wifi_hotspot = true};
  ReinitBwuManager(allowed);

  // Connect kEndpointId2 over WIFI_LAN so isWifiLanConnected() is true.
  CreateInitialEndpoint(&client_, kServiceIdA, kEndpointId2, Medium::WIFI_LAN);
  ASSERT_TRUE(ecm_.isWifiLanConnected());

  MediumRole remote_medium_role;
  CreateConnectedEndpoint(kEndpointId1, remote_medium_role, allowed);

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1));
  ASSERT_EQ(fake_wifi_lan_bwu_handler_->handle_initialize_calls().size(), 1u);

  // Simulate WIFI_LAN failure on kEndpointId1; TryNextBestUpgradeMediums picks
  // WIFI_HOTSPOT, which early-returns in "bwu-init" due to isWifiLanConnected.
  BandwidthUpgradeNegotiationFrame::UpgradePathInfo failure_info;
  failure_info.set_medium(
      BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WIFI_LAN);
  ExceptionOr<OfflineFrame> upgrade_failure =
      parser::FromBytes(parser::ForBwuFailure(failure_info));
  ASSERT_TRUE(upgrade_failure.ok());
  bwu_manager_->OnIncomingFrame(upgrade_failure.result(),
                                std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  EXPECT_TRUE(
      fake_wifi_hotspot_bwu_handler_->handle_initialize_calls().empty());
  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));

  // Because GetBwuMediumForEndpoint(kEndpointId1) was not prematurely set to
  // WIFI_HOTSPOT before the early return, an incoming WIFI_LAN path available
  // frame is accepted.
  OfflineFrame available_frame =
      CreateWifiLanPathAvailableFrameWithoutClientIntroAck();
  bwu_manager_->OnIncomingFrame(available_frame, std::string(kEndpointId1),
                                &client_, Medium::BLUETOOTH);

  EXPECT_EQ(fake_wifi_lan_bwu_handler_->create_calls().size(), 1u);
  EXPECT_TRUE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));

  UnRegisterChannelForEndpoint(kEndpointId1);
  UnRegisterChannelForEndpoint(kEndpointId2);
}

// Verifies that a Windows advertiser that delegated hosting via
// UPGRADE_PATH_REQUEST still ignores an UPGRADE_PATH_AVAILABLE frame for a
// medium it cannot join as a role-switch client (WIFI_LAN), so delegation only
// widens acceptance to client-capable mediums.
TEST_F(BwuManagerTest,
       ProcessBwuPathAvailableDelegatedAdvertiserIgnoresNonClientRoleMedium) {
  SetSupportMultipleBwuMediums(true);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  const BooleanMediumSelector allowed{.wifi_lan = true, .wifi_direct = true};
  ReinitBwuManager(allowed);

  client_.SetLocalOsType(OsInfo::WINDOWS);
  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_direct_group_owner(true);
  FakeEndpointChannel* channel_ptr =
      CreateConnectedEndpoint(kEndpointId1, remote_medium_role, allowed,
                              OsInfo::ANDROID, /*is_incoming_connection=*/true);

  // Delegate WIFI_DIRECT hosting to the Android peer.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WIFI_DIRECT);
  ASSERT_EQ(channel_ptr->GetWrittenFrames().size(), 1u);
  ExceptionOr<OfflineFrame> request_frame =
      parser::FromBytes(channel_ptr->GetWrittenFrames()[0]);
  ASSERT_TRUE(request_frame.ok());
  ASSERT_EQ(
      request_frame.result().v1().bandwidth_upgrade_negotiation().event_type(),
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_REQUEST);

  OfflineFrame available_frame =
      CreateWifiLanPathAvailableFrameWithoutClientIntroAck();
  bwu_manager_->OnIncomingFrame(available_frame, std::string(kEndpointId1),
                                &client_, Medium::BLUETOOTH);

  EXPECT_TRUE(fake_wifi_lan_bwu_handler_->create_calls().empty());
  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that an Apple discoverer connected to an Android peer does not
// host a local WifiAwareBwuHandler publisher when InitiateBwuForEndpoint is
// called, because iOS's PIN_CODE_DISPLAY publisher cannot pair with an Android
// subscriber and starting a local publisher would mark in_progress_upgrades_
// and reject Android's incoming UPGRADE_PATH_AVAILABLE frame.
TEST_F(
    BwuManagerTest,
    InitiateBwuAutoSelectAppleAwareR4AndroidPeerDiscovererDoesNotHostLocally) {
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  bwu_manager_->Shutdown();
  mediums_ = std::make_unique<Mediums>();
  std::unique_ptr<BwuHandler> wifi_aware_bwu_handler(
      mediums_->GetWifiAware().CreateBwuHandler(nullptr));
  absl::flat_hash_map<Medium, std::unique_ptr<BwuHandler>> handlers;
  handlers.emplace(Medium::WIFI_AWARE_R4, std::move(wifi_aware_bwu_handler));
  BwuManager::Config config;
  config.allow_upgrade_to = BooleanMediumSelector{.wifi_aware_r4 = true};
  bwu_manager_ = std::make_unique<BwuManager>(*mediums_, em_, ecm_,
                                              std::move(handlers), config);
  bwu_manager_->MakeSingleThreadedForTesting();

  client_.SetLocalOsType(OsInfo::APPLE);
  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_aware_publisher(true);
  FakeEndpointChannel* channel_ptr = CreateConnectedEndpoint(
      kEndpointId1, remote_medium_role,
      BooleanMediumSelector{.wifi_aware_r4 = true}, OsInfo::ANDROID,
      /*is_incoming_connection=*/false);

  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1));

  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));
  EXPECT_FALSE(mediums_->GetWifiAware().IsAcceptingConnections(
      WrapInitiatorUpgradeServiceId(kServiceIdA)));
  EXPECT_TRUE(channel_ptr->GetWrittenFrames().empty());

  UnRegisterChannelForEndpoint(kEndpointId1);
}

// Verifies that when a delegated upgrade fails via ProcessUpgradeFailureEvent
// and TryNextBestUpgradeMediums schedules a delayed retry without calling
// InitiateBwuForEndpoint, delegated_upgrade_endpoints_ is cleared so the
// advertiser does not accept a subsequent unsolicited UPGRADE_PATH_AVAILABLE
// frame for a non-NeedToSwitchRole medium (WIFI_HOTSPOT).
TEST_F(BwuManagerTest,
       ProcessUpgradeFailureClearsDelegatedUpgradeEndpointState) {
  client_.SetLocalOsType(OsInfo::WINDOWS);
  SetSupportMultipleBwuMediums(true);
  NearbyFlags::GetInstance().OverrideBoolFlagValue(
      config_package_nearby::nearby_connections_feature::
          kEnableDynamicRoleSwitch,
      true);
  ReinitBwuManager(
      BooleanMediumSelector{.wifi_hotspot = true, .wifi_direct = true});

  MediumRole remote_medium_role;
  remote_medium_role.set_support_wifi_direct_group_owner(true);
  FakeEndpointChannel* channel_ptr =
      CreateConnectedEndpoint(kEndpointId1, remote_medium_role,
                              BooleanMediumSelector{.wifi_direct = true},
                              OsInfo::ANDROID, /*is_incoming_connection=*/true);

  // Delegate WIFI_DIRECT hosting to the Android peer.
  bwu_manager_->InitiateBwuForEndpoint(&client_, std::string(kEndpointId1),
                                       Medium::WIFI_DIRECT);
  ASSERT_EQ(channel_ptr->GetWrittenFrames().size(), 1u);
  ExceptionOr<OfflineFrame> request_frame =
      parser::FromBytes(channel_ptr->GetWrittenFrames()[0]);
  ASSERT_TRUE(request_frame.ok());
  ASSERT_EQ(
      request_frame.result().v1().bandwidth_upgrade_negotiation().event_type(),
      BandwidthUpgradeNegotiationFrame::UPGRADE_PATH_REQUEST);

  // Simulate UPGRADE_FAILURE for WIFI_DIRECT; since no untried mediums remain
  // for kEndpointId1, TryNextBestUpgradeMediums schedules a delayed retry
  // without calling InitiateBwuForEndpoint.
  BandwidthUpgradeNegotiationFrame::UpgradePathInfo failure_info;
  failure_info.set_medium(
      BandwidthUpgradeNegotiationFrame::UpgradePathInfo::WIFI_DIRECT);
  ExceptionOr<OfflineFrame> upgrade_failure =
      parser::FromBytes(parser::ForBwuFailure(failure_info));
  ASSERT_TRUE(upgrade_failure.ok());
  bwu_manager_->OnIncomingFrame(upgrade_failure.result(),
                                std::string(kEndpointId1), &client_,
                                Medium::BLUETOOTH);

  // Because ProcessUpgradeFailureEvent cleared delegated_upgrade_endpoints_,
  // an unsolicited WIFI_HOTSPOT UPGRADE_PATH_AVAILABLE frame is ignored by the
  // Windows advertiser (NeedToSwitchRole is false for WIFI_HOTSPOT on Windows).
  OfflineFrame available_frame =
      CreateWifiHotspotPathAvailableFrameWithoutClientIntroAck();
  bwu_manager_->OnIncomingFrame(available_frame, std::string(kEndpointId1),
                                &client_, Medium::BLUETOOTH);

  EXPECT_TRUE(fake_wifi_hotspot_bwu_handler_->create_calls().empty());
  EXPECT_FALSE(bwu_manager_->IsUpgradeOngoing(std::string(kEndpointId1)));

  UnRegisterChannelForEndpoint(kEndpointId1);
}

}  // namespace
}  // namespace nearby::connections
