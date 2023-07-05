// Copyright 2023 Google LLC
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

#include "fastpair/fast_pair_service.h"

#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "fastpair/fast_pair_events.h"
#include "fastpair/internal/fast_pair_seeker_impl.h"
#include "fastpair/message_stream/fake_provider.h"
#include "fastpair/plugins/fake_fast_pair_plugin.h"
#include "internal/account/fake_account_manager.h"
#include "internal/network/http_client.h"
#include "internal/platform/device_info.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"
#include "internal/test/fake_device_info.h"
#include "internal/test/fake_http_client.h"
#include "internal/test/google3_only/fake_authentication_manager.h"

namespace nearby {
namespace fastpair {

namespace {
constexpr absl::string_view kPluginName = "my plugin";
constexpr absl::string_view kModelId{"718c17"};
constexpr absl::string_view kPublicAntiSpoof =
    "Wuyr48lD3txnUhGiMF1IfzlTwRxxe+wMB1HLzP+"
    "0wVcljfT3XPoiy1fntlneziyLD5knDVAJSE+RM/zlPRP/Jg==";
using ::testing::status::StatusIs;

class FastPairServiceTest : public ::testing::Test {
 protected:
  FastPairServiceTest() {
    AccountManagerImpl::Factory::SetFactoryForTesting(
        &account_manager_factory_);
    http_client_ = std::make_unique<network::FakeHttpClient>();
    device_info_ = std::make_unique<FakeDeviceInfo>();
    authentication_manager_ =
        std::make_unique<nearby::FakeAuthenticationManager>();
  }

  void SetUp() override {
    MediumEnvironment::Instance().Start();
    GetAuthManager()->EnableSyncMode();
  }

  void TearDown() override { MediumEnvironment::Instance().Stop(); }

  nearby::FakeAuthenticationManager* GetAuthManager() {
    return reinterpret_cast<nearby::FakeAuthenticationManager*>(
        authentication_manager_.get());
  }

  network::FakeHttpClient* GetHttpClient() {
    return reinterpret_cast<network::FakeHttpClient*>(http_client_.get());
  }

  void SetUpDeviceMetadata() {
    proto::GetObservedDeviceResponse response_proto;
    auto* device = response_proto.mutable_device();
    int64_t device_id;
    CHECK(absl::SimpleHexAtoi(kModelId, &device_id));
    device->set_id(device_id);
    network::HttpResponse response;
    response.SetStatusCode(network::HttpStatusCode::kHttpOk);
    response.SetBody(response_proto.SerializeAsString());
    GetHttpClient()->SetResponseForSyncRequest(response);
  }

  FakeAccountManager::Factory account_manager_factory_;
  std::unique_ptr<auth::AuthenticationManager> authentication_manager_;
  std::unique_ptr<network::HttpClient> http_client_;
  std::unique_ptr<DeviceInfo> device_info_;
};

TEST(FastPairService, RegisterUnregister) {
  FastPairService service;

  EXPECT_OK(service.RegisterPluginProvider(
      kPluginName, std::make_unique<FakeFastPairPluginProvider>()));
  EXPECT_OK(service.UnregisterPluginProvider(kPluginName));
}

TEST(FastPairService, RegisterTwiceFails) {
  FastPairService service;
  EXPECT_OK(service.RegisterPluginProvider(
      kPluginName, std::make_unique<FakeFastPairPluginProvider>()));

  EXPECT_THAT(service.RegisterPluginProvider(
                  kPluginName, std::make_unique<FakeFastPairPluginProvider>()),
              StatusIs(absl::StatusCode::kAlreadyExists));
}

TEST(FastPairService, UnregisterTwiceFails) {
  FastPairService service;
  EXPECT_OK(service.RegisterPluginProvider(
      kPluginName, std::make_unique<FakeFastPairPluginProvider>()));
  EXPECT_OK(service.UnregisterPluginProvider(kPluginName));

  EXPECT_THAT(service.UnregisterPluginProvider(kPluginName),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(FastPairServiceTest, InitialDiscoveryEvent) {
  FakeProvider provider;
  SetUpDeviceMetadata();
  FastPairService service(std::move(authentication_manager_),
                          std::move(http_client_), std::move(device_info_));

  CountDownLatch latch(1);
  auto plugin_provider = std::make_unique<FakeFastPairPluginProvider>();
  plugin_provider->on_initial_discovery_event_ =
      [&](const FastPairDevice* device, const InitialDiscoveryEvent& event) {
        NEARBY_LOGS(INFO) << "Initial discovery: " << device;
        EXPECT_EQ(device->GetModelId(), kModelId);
        latch.CountDown();
      };
  EXPECT_OK(
      service.RegisterPluginProvider(kPluginName, std::move(plugin_provider)));
  FastPairSeekerExt* seeker =
      static_cast<FastPairSeekerExt*>(service.GetSeeker());

  EXPECT_OK(seeker->StartFastPairScan());
  provider.StartDiscoverableAdvertisement(kModelId);
  latch.Await();

  EXPECT_OK(seeker->StopFastPairScan());
  EXPECT_OK(service.UnregisterPluginProvider(kPluginName));
}

TEST_F(FastPairServiceTest, ScreenEvent) {
  FakeProvider provider;
  SetUpDeviceMetadata();
  FastPairService service(std::move(authentication_manager_),
                          std::move(http_client_), std::move(device_info_));

  CountDownLatch latch(1);
  auto plugin_provider = std::make_unique<FakeFastPairPluginProvider>();
  plugin_provider->on_initial_discovery_event_ =
      [&](const FastPairDevice* device, const InitialDiscoveryEvent& event) {
        NEARBY_LOGS(INFO) << "Initial discovery: " << device;
        EXPECT_EQ(device->GetModelId(), kModelId);
        latch.CountDown();
      };
  plugin_provider->on_screen_event_ = [&](ScreenEvent event) {
    EXPECT_TRUE(event.is_locked);
    latch.CountDown();
  };
  EXPECT_OK(
      service.RegisterPluginProvider(kPluginName, std::move(plugin_provider)));
  FastPairSeekerExt* seeker =
      static_cast<FastPairSeekerExt*>(service.GetSeeker());
  EXPECT_OK(seeker->StartFastPairScan());
  provider.StartDiscoverableAdvertisement(kModelId);
  seeker->SetIsScreenLocked(true);
  latch.Await();

  EXPECT_OK(seeker->StopFastPairScan());
  EXPECT_OK(service.UnregisterPluginProvider(kPluginName));
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
