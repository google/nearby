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

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "fastpair/fast_pair_plugin.h"
#include "fastpair/internal/fast_pair_seeker_impl.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

namespace {
using ::testing::status::StatusIs;

class FakeFastPairPluginProvider : public FastPairPluginProvider {
 public:
  std::unique_ptr<FastPairPlugin> GetPlugin(
      FastPairSeeker* seeker, const FastPairDevice* device) override {
    return std::make_unique<FastPairPlugin>();
  }
};

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

TEST(FastPairService, FakeInitialPairing) {
  constexpr absl::string_view kPluginName = "my plugin";
  FastPairService service;
  EXPECT_OK(service.RegisterPluginProvider(
      kPluginName, std::make_unique<FakeFastPairPluginProvider>()));
  FastPairSeekerExt* seeker =
      static_cast<FastPairSeekerExt*>(service.GetSeeker());

  EXPECT_OK(seeker->StartFastPairScan());
  EXPECT_OK(seeker->StopFastPairScan());

  EXPECT_OK(service.UnregisterPluginProvider(kPluginName));
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
