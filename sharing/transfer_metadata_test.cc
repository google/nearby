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

#include "sharing/transfer_metadata.h"

#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace nearby {
namespace sharing {
namespace {

struct TransferMetadataToStringTestData {
  TransferMetadata transfer_metadata;
  std::string expected_string_result;
};

std::vector<TransferMetadataToStringTestData> GetTestData() {
  static std::vector<TransferMetadataToStringTestData>*
      kTransferMetadataToStringTestData =
          new std::vector<TransferMetadataToStringTestData>({
              {TransferMetadata(
                   TransferMetadata::Status::kConnecting,
                   /*progress=*/12.321f, /*token=*/std::nullopt,
                   /*is_original=*/true, /*is_final_status=*/false,
                   /*is_self_share=*/false,
                   /*transferred_bytes=*/123456789,
                   /*transfer_speed=*/300000,
                   /*estimated_time_remaining=*/123456,
                   /*total_attachments_count=*/1,
                   /*transferred_attachments_count=*/0,
                   /*in_progress_attachment_id=*/std::nullopt,
                   /*in_progress_attachment_transferred_bytes=*/std::nullopt,
                   /*in_progress_attachment_total_bytes=*/std::nullopt),
               "TransferMetadata<status: kConnecting, progress: 12.32, "
               "is_original: 1, is_final_status: 0, is_self_share: 0, "
               "transferred_bytes: 123456789, transfer_speed: 300000, "
               "estimated_time_remaining: 123456>"},
              {TransferMetadata(
                   TransferMetadata::Status::kCancelled,
                   /*progress=*/77.795f,
                   std::optional<std::string>{"test_token"},
                   /*is_original=*/false,
                   /*is_final_status=*/true, /*is_self_share=*/true,
                   /*transferred_bytes=*/123456789, /*transfer_speed=*/0,
                   /*estimated_time_remaining=*/123456789,
                   /*total_attachments_count=*/1,
                   /*transferred_attachments_count=*/0,
                   /*in_progress_attachment_id=*/std::nullopt,
                   /*in_progress_attachment_transferred_bytes=*/std::nullopt,
                   /*in_progress_attachment_total_bytes=*/std::nullopt),
               "TransferMetadata<status: kCancelled, progress: 77.79, token: "
               "test_token, is_original: 0, is_final_status: 1, is_self_share: "
               "1, transferred_bytes: 123456789, transfer_speed: 0, "
               "estimated_time_remaining: 123456789>"},
          });

  return *kTransferMetadataToStringTestData;
}

using TransferMetadataToStringTest =
    testing::TestWithParam<TransferMetadataToStringTestData>;

TEST_P(TransferMetadataToStringTest, ToStringResultMatches) {
  EXPECT_EQ(GetParam().expected_string_result,
            GetParam().transfer_metadata.ToString());
}

INSTANTIATE_TEST_CASE_P(TransferMetadataToStringTest,
                        TransferMetadataToStringTest,
                        testing::ValuesIn(GetTestData()));

}  // namespace
}  // namespace sharing
}  // namespace nearby
