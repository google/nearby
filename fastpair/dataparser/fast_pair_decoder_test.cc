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

#include "fastpair/dataparser/fast_pair_decoder.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "absl/strings/escaping.h"
#include "fastpair/testing/fast_pair_service_data_creator.h"

namespace nearby {
namespace fastpair {
namespace {
constexpr char kModelId[7] = "aabbcc";
constexpr char kLongModelId[17] = "1122334455667788";
constexpr char kPaddedModelId[9] = "00001111";
constexpr char kTrimmedModelId[7] = "001111";
constexpr uint8_t kLongModelIdHeader = 0b00010000;
constexpr uint8_t kPaddedLongModelIdHeader = 0b00001000;

bool HasModelIdString(std::string model_id) {
  std::string bytes_str = absl::HexStringToBytes(model_id);
  std::vector<uint8_t> model_id_bytes;
  std::move(std::begin(bytes_str), std::end(bytes_str),
            std::back_inserter(model_id_bytes));
  return FastPairDecoder::HasModelId(&model_id_bytes);
}

TEST(FastPairDecoderTest, HasModelIdThreeByteFormat) {
  EXPECT_TRUE(HasModelIdString(kModelId));
}

TEST(FastPairDecoderTest, HasModelIdTooShort) {
  EXPECT_FALSE(HasModelIdString("11"));
}

TEST(FastPairDecoderTest, HasModelIdLongFormat) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(0b00001000)
                                   .SetModelId("11223344")
                                   .Build()
                                   ->CreateServiceData();
  EXPECT_TRUE(FastPairDecoder::HasModelId(&bytes));

  bytes = FastPairServiceDataCreator::Builder()
              .SetHeader(0b00001010)
              .SetModelId("1122334455")
              .Build()
              ->CreateServiceData();

  EXPECT_TRUE(FastPairDecoder::HasModelId(&bytes));
}

TEST(FastPairDecoderTest, HasModelIdLongInvalidVersion) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(0b00101000)
                                   .SetModelId("11223344")
                                   .Build()
                                   ->CreateServiceData();
  EXPECT_FALSE(FastPairDecoder::HasModelId(&bytes));
}

TEST(FastPairDecoderTest, HasModelIdLongInvalidLength) {
  std::vector<uint8_t> bytes = FastPairServiceDataCreator::Builder()
                                   .SetHeader(0b00001010)
                                   .SetModelId("11223344")
                                   .Build()
                                   ->CreateServiceData();
  EXPECT_FALSE(FastPairDecoder::HasModelId(&bytes));

  bytes = FastPairServiceDataCreator::Builder()
              .SetHeader(0b00000010)
              .SetModelId("11223344")
              .Build()
              ->CreateServiceData();

  EXPECT_FALSE(FastPairDecoder::HasModelId(&bytes));
}

TEST(FastPairDecoderTest, GetHexModelIdFromServiceDataNoResultForNullData) {
  EXPECT_EQ(FastPairDecoder::GetHexModelIdFromServiceData(nullptr),
            absl::nullopt);
}

TEST(FastPairDecoderTest, GetHexModelIdFromServiceDataNoResultForEmptyData) {
  std::vector<uint8_t> empty;
  EXPECT_EQ(FastPairDecoder::GetHexModelIdFromServiceData(&empty),
            absl::nullopt);
}

TEST(FastPairDecoderTest, GetHexModelIdFromServiceDataThreeByteData) {
  std::vector<uint8_t> service_data = FastPairServiceDataCreator::Builder()
                                          .SetModelId(kModelId)
                                          .Build()
                                          ->CreateServiceData();
  EXPECT_EQ(FastPairDecoder::GetHexModelIdFromServiceData(&service_data),
            kModelId);
}

TEST(FastPairDecoderTest, GetHexModelIdFromServiceDataLongModelId) {
  std::vector<uint8_t> service_data = FastPairServiceDataCreator::Builder()
                                          .SetHeader(kLongModelIdHeader)
                                          .SetModelId(kLongModelId)
                                          .Build()
                                          ->CreateServiceData();

  EXPECT_EQ(FastPairDecoder::GetHexModelIdFromServiceData(&service_data),
            kLongModelId);
}

TEST(FastPairDecoderTest, GetHexModelIdFromServiceDataLongModelIdTrimmed) {
  std::vector<uint8_t> service_data = FastPairServiceDataCreator::Builder()
                                          .SetHeader(kPaddedLongModelIdHeader)
                                          .SetModelId(kPaddedModelId)
                                          .Build()
                                          ->CreateServiceData();

  EXPECT_EQ(FastPairDecoder::GetHexModelIdFromServiceData(&service_data),
            kTrimmedModelId);
}

}  // namespace
}  // namespace fastpair
}  // namespace nearby
