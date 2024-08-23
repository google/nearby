// Copyright 2024 Google LLC
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

#include "sharing/wrapped_share_target_discovered_callback.h"

#include <cstdint>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "sharing/advertisement.h"
#include "sharing/share_target.h"
#include "sharing/share_target_discovered_callback.h"

namespace nearby {
namespace sharing {
namespace {

using ::testing::_;
using BlockedVendorId = Advertisement::BlockedVendorId;

class MockShareTargetDiscoveredCallback : public ShareTargetDiscoveredCallback {
 public:
  MOCK_METHOD(void, OnShareTargetDiscovered, (const ShareTarget&), (override));
  MOCK_METHOD(void, OnShareTargetUpdated, (const ShareTarget&), (override));
  MOCK_METHOD(void, OnShareTargetLost, (const ShareTarget&), (override));
};

ShareTarget GetShareTarget(uint8_t vendor_id) {
  ShareTarget share_target;
  share_target.vendor_id = vendor_id;
  return share_target;
}

TEST(WrappedShareTargetDiscoveredCallbackTest, BlocksDiscoveryForSameVendorId) {
  MockShareTargetDiscoveredCallback callback;
  ShareTarget share_target = GetShareTarget(/*vendor_id=*/1);
  WrappedShareTargetDiscoveredCallback wrapped(&callback,
                                               BlockedVendorId::kSamsung,
                                               /*disable_wifi_hotspot=*/false);
  EXPECT_CALL(callback, OnShareTargetDiscovered(_)).Times(0);
  wrapped.OnShareTargetDiscovered(share_target);
}

TEST(WrappedShareTargetDiscoveredCallbackTest, BlocksUpdatedForSameVendorId) {
  MockShareTargetDiscoveredCallback callback;
  ShareTarget share_target = GetShareTarget(/*vendor_id=*/1);
  WrappedShareTargetDiscoveredCallback wrapped(&callback,
                                               BlockedVendorId::kSamsung,
                                               /*disable_wifi_hotspot=*/false);
  EXPECT_CALL(callback, OnShareTargetUpdated(_)).Times(0);
  wrapped.OnShareTargetUpdated(share_target);
}

TEST(WrappedShareTargetDiscoveredCallbackTest, BlocksLostForSameVendorId) {
  MockShareTargetDiscoveredCallback callback;
  ShareTarget share_target = GetShareTarget(/*vendor_id=*/1);
  WrappedShareTargetDiscoveredCallback wrapped(&callback,
                                               BlockedVendorId::kSamsung,
                                               /*disable_wifi_hotspot=*/false);
  EXPECT_CALL(callback, OnShareTargetLost(_)).Times(0);
  wrapped.OnShareTargetLost(share_target);
}

TEST(WrappedShareTargetDiscoveredCallbackTest,
     DoesNotBlockDiscoveryForDifferentVendorId) {
  MockShareTargetDiscoveredCallback callback;
  ShareTarget share_target = GetShareTarget(/*vendor_id=*/0);
  WrappedShareTargetDiscoveredCallback wrapped(&callback,
                                               BlockedVendorId::kSamsung,
                                               /*disable_wifi_hotspot=*/false);
  EXPECT_CALL(callback, OnShareTargetLost(_));
  wrapped.OnShareTargetLost(share_target);
}

TEST(WrappedShareTargetDiscoveredCallbackTest,
     DoesNotBlockUpdatedForDifferentVendorId) {
  MockShareTargetDiscoveredCallback callback;
  ShareTarget share_target = GetShareTarget(/*vendor_id=*/0);
  WrappedShareTargetDiscoveredCallback wrapped(&callback,
                                               BlockedVendorId::kSamsung,
                                               /*disable_wifi_hotspot=*/false);
  EXPECT_CALL(callback, OnShareTargetLost(_));
  wrapped.OnShareTargetLost(share_target);
}

TEST(WrappedShareTargetDiscoveredCallbackTest,
     DoesNotBlockLostForDifferentVendorId) {
  MockShareTargetDiscoveredCallback callback;
  ShareTarget share_target = GetShareTarget(/*vendor_id=*/0);
  WrappedShareTargetDiscoveredCallback wrapped(&callback,
                                               BlockedVendorId::kSamsung,
                                               /*disable_wifi_hotspot=*/false);
  EXPECT_CALL(callback, OnShareTargetLost(_));
  wrapped.OnShareTargetLost(share_target);
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
