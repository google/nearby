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

#include "sharing/nearby_connections_types.h"

#include "gtest/gtest.h"
#include "connections/status.h"
#include "sharing/nearby_connections_manager.h"
#include "sharing/nearby_connections_service.h"

namespace nearby {
namespace sharing {
namespace {

using NsStatus = nearby::sharing::Status;
using NcStatus = nearby::connections::Status;

// LINT.IfChange(status_enum)
TEST(NearbyConnectionSharingTypesTest, TestStatusValueIsSame) {
  EXPECT_EQ(NsStatus::kSuccess, ConvertToStatus({NcStatus::kSuccess}));
  EXPECT_EQ(NsStatus::kError, ConvertToStatus({NcStatus::kError}));
  EXPECT_EQ(NsStatus::kOutOfOrderApiCall,
            ConvertToStatus({NcStatus::kOutOfOrderApiCall}));
  EXPECT_EQ(NsStatus::kAlreadyHaveActiveStrategy,
            ConvertToStatus({NcStatus::kAlreadyHaveActiveStrategy}));
  EXPECT_EQ(NsStatus::kAlreadyAdvertising,
            ConvertToStatus({NcStatus::kAlreadyAdvertising}));
  EXPECT_EQ(NsStatus::kAlreadyDiscovering,
            ConvertToStatus({NcStatus::kAlreadyDiscovering}));
  EXPECT_EQ(NsStatus::kEndpointIOError,
            ConvertToStatus({NcStatus::kEndpointIoError}));
  EXPECT_EQ(NsStatus::kEndpointUnknown,
            ConvertToStatus({NcStatus::kEndpointUnknown}));
  EXPECT_EQ(NsStatus::kConnectionRejected,
            ConvertToStatus({NcStatus::kConnectionRejected}));
  EXPECT_EQ(NsStatus::kAlreadyConnectedToEndpoint,
            ConvertToStatus({NcStatus::kAlreadyConnectedToEndpoint}));
  EXPECT_EQ(NsStatus::kAlreadyListening,
            ConvertToStatus({NcStatus::kAlreadyListening}));
  EXPECT_EQ(NsStatus::kNotConnectedToEndpoint,
            ConvertToStatus({NcStatus::kNotConnectedToEndpoint}));
  EXPECT_EQ(NsStatus::kBluetoothError,
            ConvertToStatus({NcStatus::kBluetoothError}));
  EXPECT_EQ(NsStatus::kBleError, ConvertToStatus({NcStatus::kBleError}));
  EXPECT_EQ(NsStatus::kWifiLanError,
            ConvertToStatus({NcStatus::kWifiLanError}));
  EXPECT_EQ(NsStatus::kPayloadUnknown,
            ConvertToStatus({NcStatus::kPayloadUnknown}));
  EXPECT_EQ(NsStatus::kReset, ConvertToStatus({NcStatus::kReset}));
  EXPECT_EQ(NsStatus::kTimeout, ConvertToStatus({NcStatus::kTimeout}));
  EXPECT_EQ(NsStatus::kUnknown, ConvertToStatus({NcStatus::kUnknown}));
  EXPECT_EQ(NsStatus::kNextValue, ConvertToStatus({NcStatus::kNextValue}));
}
// LINT.ThenChange()

TEST(NearbyConnectionSharingTypesTest, TestNoFailWithUnknownStatus) {
  EXPECT_EQ(
      NearbyConnectionsManager::ConnectionsStatusToString(NsStatus::kNextValue),
      "Unknown");
}

}  // namespace
}  // namespace sharing
}  // namespace nearby
