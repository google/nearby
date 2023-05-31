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

#include "fastpair/plugins/fake_initial_pair_plugin.h"

#include <memory>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "fastpair/common/protocol.h"
#include "fastpair/mock_fast_pair_seeker.h"

namespace nearby {
namespace fastpair {
namespace {

using ::testing::Return;

}  // namespace

TEST(FakeInitialPairPluginTest, InitialPair) {
  constexpr absl::string_view kModelId = "123456";
  constexpr absl::string_view kBleAddress = "F1:F2:F3:F4:F5:F6";
  MockFastPairSeeker seeker;
  FastPairDevice device(kModelId, kBleAddress,
                        Protocol::kFastPairInitialPairing);
  EXPECT_CALL(seeker, StartInitialPairing)
      .Times(1)
      .WillOnce(Return(absl::OkStatus()));

  FakeInitialPairPlugin::Provider provider;
  std::unique_ptr<FastPairPlugin> plugin = provider.GetPlugin(&seeker, &device);
  plugin->OnInitialDiscoveryEvent(InitialDiscoveryEvent{});

  plugin.reset();
}
}  // namespace fastpair
}  // namespace nearby
