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

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "fastpair/fast_pair_events.h"
#include "fastpair/internal/fast_pair_seeker_impl.h"
#include "fastpair/message_stream/fake_provider.h"
#include "fastpair/plugins/fake_fast_pair_plugin.h"
#include "fastpair/server_access/fake_fast_pair_repository.h"
#include "internal/platform/logging.h"
#include "internal/platform/medium_environment.h"

namespace nearby {
namespace fastpair {

namespace {
constexpr absl::string_view kModelId{"718c17"};
constexpr absl::string_view kPublicAntiSpoof =
    "Wuyr48lD3txnUhGiMF1IfzlTwRxxe+wMB1HLzP+"
    "0wVcljfT3XPoiy1fntlneziyLD5knDVAJSE+RM/zlPRP/Jg==";

using ::testing::status::StatusIs;

TEST(FastPairService, RegisterUnregister) {
  constexpr absl::string_view kPluginName = "my plugin";
  FastPairService service;

  EXPECT_OK(service.RegisterPluginProvider(
      kPluginName, std::make_unique<FakeFastPairPluginProvider>()));
  EXPECT_OK(service.UnregisterPluginProvider(kPluginName));
}

TEST(FastPairService, RegisterTwiceFails) {
  constexpr absl::string_view kPluginName = "my plugin";
  FastPairService service;
  EXPECT_OK(service.RegisterPluginProvider(
      kPluginName, std::make_unique<FakeFastPairPluginProvider>()));

  EXPECT_THAT(service.RegisterPluginProvider(
                  kPluginName, std::make_unique<FakeFastPairPluginProvider>()),
              StatusIs(absl::StatusCode::kAlreadyExists));
}

TEST(FastPairService, UnregisterTwiceFails) {
  constexpr absl::string_view kPluginName = "my plugin";
  FastPairService service;
  EXPECT_OK(service.RegisterPluginProvider(
      kPluginName, std::make_unique<FakeFastPairPluginProvider>()));
  EXPECT_OK(service.UnregisterPluginProvider(kPluginName));

  EXPECT_THAT(service.UnregisterPluginProvider(kPluginName),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST(FastPairService, InitialDiscoveryEvent) {
  MediumEnvironment::Instance().Start();
  constexpr absl::string_view kPluginName = "my plugin";
  auto repository = FakeFastPairRepository::Create(kModelId, kPublicAntiSpoof);
  FakeProvider provider;
  FastPairService service;
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
  MediumEnvironment::Instance().Stop();
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
