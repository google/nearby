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

#include "sharing/nearby_connections_service.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/platform/file.h"
#include "sharing/nearby_connections_types.h"

namespace nearby::sharing {
namespace {

using ::testing::Eq;

TEST(NearbyConnectionSharingServicePayloadTest, ConvertBytesToPayload) {
  Payload payload = ConvertToPayload(NcPayload(1234, NcByteArray("test")));
  EXPECT_THAT(payload.id, Eq(1234LL));
  EXPECT_THAT(payload.content.type, Eq(PayloadContent::Type::kBytes));
}

TEST(NearbyConnectionSharingServicePayloadTest, ConvertFileToPayload) {
  Payload payload = ConvertToPayload(
      NcPayload(1234, nearby::InputFile("/为甚么/tmp/test.txt", /*size=*/100)));
  EXPECT_THAT(payload.id, Eq(1234LL));
  EXPECT_THAT(payload.content.type, Eq(PayloadContent::Type::kFile));
}

}  // namespace
}  // namespace nearby::sharing
