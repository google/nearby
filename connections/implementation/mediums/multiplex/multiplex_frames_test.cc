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

#include "connections/implementation/mediums/multiplex/multiplex_frames.h"
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace multiplex {

using ::location::nearby::mediums::MultiplexFrame;
using ::location::nearby::mediums::MultiplexControlFrame;
using ::location::nearby::mediums::ConnectionResponseFrame;

constexpr absl::string_view kServiceId_1 = "serviceId_1";
constexpr absl::string_view kServiceId_2 = "serviceId_2";

TEST(MultiplexFrameTest, FrameValidation) {
  const ByteArray data("abcdefghijklmnopqrstuvwxyz");
  MultiplexFrame frame;
  EXPECT_FALSE(IsValid(frame));
  frame.set_frame_type(MultiplexFrame::CONTROL_FRAME);
  EXPECT_FALSE(IsValidControlFrame(frame));

  auto* control_frame = frame.mutable_control_frame();
  control_frame->set_control_frame_type(
      MultiplexControlFrame::UNKNOWN_CONTROL_FRAME_TYPE);
  EXPECT_FALSE(IsValidControlFrame(frame));
  auto* header = frame.mutable_header();
  header->set_salted_service_id_hash(std::string(
      GenerateServiceIdHashWithSalt(std::string(kServiceId_1), "1234")));
  control_frame->set_control_frame_type(
      MultiplexControlFrame::CONNECTION_REQUEST);
  EXPECT_TRUE(IsValidControlFrame(frame));
  EXPECT_TRUE(IsValid(frame));
  control_frame->set_control_frame_type(
      MultiplexControlFrame::CONNECTION_RESPONSE);
  EXPECT_TRUE(IsValidControlFrame(frame));
  EXPECT_TRUE(IsValid(frame));
  control_frame->set_control_frame_type(
      MultiplexControlFrame::DISCONNECTION);
  EXPECT_TRUE(IsValidControlFrame(frame));
  EXPECT_TRUE(IsValid(frame));

  EXPECT_FALSE(IsValidDataFrame(frame));
  frame.set_frame_type(MultiplexFrame::DATA_FRAME);
  auto* data_frame = frame.mutable_data_frame();
  data_frame->set_data(std::string(std::move(data)));
  EXPECT_TRUE(IsValidDataFrame(frame));
  EXPECT_TRUE(IsValid(frame));

  frame.set_frame_type(MultiplexFrame::UNKNOWN_FRAME_TYPE);
  EXPECT_FALSE(IsValid(frame));

  frame.set_frame_type(MultiplexFrame::DATA_FRAME);
  auto serialized_bytes = ByteArray(frame.SerializeAsString());
  EXPECT_TRUE(IsMultiplexFrame(std::move(serialized_bytes)));

  EXPECT_TRUE(IsControlFrame(MultiplexFrame::CONTROL_FRAME));
  EXPECT_FALSE(IsControlFrame(MultiplexFrame::DATA_FRAME));
  EXPECT_TRUE(IsDataFrame(MultiplexFrame::DATA_FRAME));
  EXPECT_FALSE(IsDataFrame(MultiplexFrame::UNKNOWN_FRAME_TYPE));
}

TEST(MultiplexFrameTest, HashValidtion) {
  auto service_id_hash_1 = GenerateServiceIdHash(std::string(kServiceId_1));
  EXPECT_EQ(service_id_hash_1.size(), kServiceIdHashLength);
  auto service_id_hash_2 = GenerateServiceIdHash(std::string(kServiceId_2));
  EXPECT_NE(service_id_hash_1, service_id_hash_2);

  auto hash_key_1 = GenerateServiceIdHashKey(service_id_hash_1);
  auto hash_key_2 = GenerateServiceIdHashKey(service_id_hash_2);
  EXPECT_NE(hash_key_1, hash_key_2);

  auto service_id_hash_with_salt_1 =
      GenerateServiceIdHashWithSalt(std::string(kServiceId_1), "1234");
  EXPECT_EQ(service_id_hash_with_salt_1.size(), kServiceIdHashLength);
  auto service_id_hash_with_salt_2 =
      GenerateServiceIdHashWithSalt(std::string(kServiceId_2), "1234");
  EXPECT_NE(service_id_hash_with_salt_1, service_id_hash_with_salt_2);
  service_id_hash_with_salt_2 =
      GenerateServiceIdHashWithSalt(std::string(kServiceId_1), "abcd");
  EXPECT_NE(service_id_hash_with_salt_1, service_id_hash_with_salt_2);

  auto hash_key_with_salt_1 =
      GenerateServiceIdHashKeyWithSalt(std::string(kServiceId_1), "1234");
  auto hash_key_with_salt_2 =
      GenerateServiceIdHashKeyWithSalt(std::string(kServiceId_2), "1234");
  EXPECT_NE(hash_key_with_salt_1, hash_key_with_salt_2);
}

TEST(MultiplexFrameTest, CanGenerateConnectionRequest) {
  ByteArray bytes = ForConnectionRequest(std::string(kServiceId_1), "1234");
  auto request = FromBytes(bytes);
  ASSERT_TRUE(request.ok());
  auto frame = request.result();
  EXPECT_EQ(frame.control_frame().control_frame_type(),
            MultiplexControlFrame::CONNECTION_REQUEST);
  EXPECT_EQ(frame.header().salted_service_id_hash(),
            std::string(GenerateServiceIdHashWithSalt(std::string(kServiceId_1),
                                                      "1234")));
}

TEST(MultiplexFrameTest, CanGenerateConnectionRespons) {
  auto service_id_hash_with_salt_2 =
      GenerateServiceIdHashWithSalt(std::string(kServiceId_2), "1234");
  ByteArray bytes =
      ForConnectionResponse(service_id_hash_with_salt_2, "1234",
                            ConnectionResponseFrame::CONNECTION_ACCEPTED);
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  auto frame = response.result();
  EXPECT_EQ(frame.control_frame().control_frame_type(),
            MultiplexControlFrame::CONNECTION_RESPONSE);
  EXPECT_EQ(frame.header().salted_service_id_hash(),
            std::string(service_id_hash_with_salt_2));
  EXPECT_EQ(frame.control_frame()
                .connection_response_frame()
                .connection_response_code(),
            ConnectionResponseFrame::CONNECTION_ACCEPTED);
}

TEST(MultiplexFrameTest, CanGenerateDisconnection) {
  ByteArray bytes = ForDisconnection(std::string(kServiceId_1), "1234");
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  auto frame = response.result();
  EXPECT_EQ(frame.control_frame().control_frame_type(),
            MultiplexControlFrame::DISCONNECTION);
  EXPECT_EQ(frame.header().salted_service_id_hash(),
            std::string(GenerateServiceIdHashWithSalt(std::string(kServiceId_1),
                                                      "1234")));
}

TEST(MultiplexFrameTest, CanGenerateData) {
  ByteArray data("abcdefghijklmnopqrstuvwxyz");
  ByteArray bytes =
      ForData(std::string(kServiceId_1), "1234", true, data);
  auto response = FromBytes(bytes);
  ASSERT_TRUE(response.ok());
  auto frame = response.result();
  EXPECT_EQ(frame.frame_type(), MultiplexFrame::DATA_FRAME);
  EXPECT_EQ(frame.header().salted_service_id_hash(),
            std::string(GenerateServiceIdHashWithSalt(std::string(kServiceId_1),
                                                      "1234")));
  EXPECT_EQ(frame.data_frame().data(),
            std::string("abcdefghijklmnopqrstuvwxyz"));
}

}  // namespace multiplex
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
