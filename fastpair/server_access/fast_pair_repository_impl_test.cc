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

#include "fastpair/server_access/fast_pair_repository_impl.h"

#include <memory>
#include <optional>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "fastpair/common/device_metadata.h"
#include "fastpair/server_access/fake_fast_pair_client.h"
#include "internal/platform/count_down_latch.h"

namespace nearby {
namespace fastpair {
namespace {
constexpr absl::string_view kHexModelId = "718C17";
constexpr absl::string_view kInitialPairingdescription =
    "InitialPairingdescription";

// A gMock matcher to match proto values. Use this matcher like:
// request/response proto, expected_proto;
// EXPECT_THAT(proto, MatchesProto(expected_proto));
MATCHER_P(
    MatchesProto, expected_proto,
    absl::StrCat(negation ? "does not match" : "matches",
                 testing::PrintToString(expected_proto.SerializeAsString()))) {
  return arg.SerializeAsString() == expected_proto.SerializeAsString();
}

TEST(FastPairRepositoryImplTest, MetadataDownloadSuccess) {
  FakeFastPairClient fake_fast_pair_client;
  auto fast_pair_repository =
      std::make_unique<FastPairRepositoryImpl>(&fake_fast_pair_client);

  // Sets up proto::GetObservedDeviceResponse
  proto::GetObservedDeviceResponse response_proto;
  auto* device = response_proto.mutable_device();
  int64_t device_id;
  CHECK(absl::SimpleHexAtoi(kHexModelId, &device_id));
  device->set_id(device_id);
  auto* observed_device_strings = response_proto.mutable_strings();
  observed_device_strings->set_initial_pairing_description(
      kInitialPairingdescription);
  fake_fast_pair_client.SetGetObservedDeviceResponse(response_proto);

  CountDownLatch latch(1);
  fast_pair_repository->GetDeviceMetadata(
      kHexModelId, [&](std::optional<DeviceMetadata> device_metadata) {
        EXPECT_TRUE(device_metadata.has_value());
        // Verifies proto::GetObservedDeviceResponse is as expected
        proto::GetObservedDeviceResponse response =
            device_metadata->GetResponse();
        EXPECT_THAT(response, MatchesProto(response_proto));
        EXPECT_EQ(response.device().id(), device_id);
        EXPECT_EQ(response.strings().initial_pairing_description(),
                  kInitialPairingdescription);
        latch.CountDown();
      });
  latch.Await();
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
