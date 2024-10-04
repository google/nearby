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

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby::sharing {
using ::testing::Eq;

TEST(NearbyConnectionSharingTypesPayloadTest, FromInputFileUTF8) {
  InputFile input_file("/为甚么/tmp/test.txt");
  Payload payload(input_file);
  EXPECT_THAT(payload.id, Eq(7724502655048749887LL));
  EXPECT_THAT(payload.content.type, Eq(PayloadContent::Type::kFile));
}

TEST(NearbyConnectionSharingTypesPayloadTest, FromInputFileWithId) {
  InputFile input_file("/为甚么/tmp/test.txt");
  Payload payload(1234, input_file);
  EXPECT_THAT(payload.id, Eq(1234LL));
  EXPECT_THAT(payload.content.type, Eq(PayloadContent::Type::kFile));
}

TEST(NearbyConnectionSharingTypesPayloadTest, FromBytes) {
  Payload payload({1, 2, 3, 4, 5});
  EXPECT_THAT(payload.content.type, Eq(PayloadContent::Type::kBytes));
}

TEST(NearbyConnectionSharingTypesPayloadTest, FromBytesWithId) {
  Payload payload(5432, {1, 2, 3, 4, 5});
  EXPECT_THAT(payload.id, Eq(5432LL));
  EXPECT_THAT(payload.content.type, Eq(PayloadContent::Type::kBytes));
}

}  // namespace nearby::sharing
