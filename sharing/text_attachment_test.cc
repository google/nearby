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

#include "sharing/text_attachment.h"

#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "sharing/proto/wire_format.pb.h"

namespace nearby {
namespace sharing {
namespace {

struct TextAttachmentTextTitleTestData {
  TextAttachment::Type type;
  std::string text_body;
  std::string expected_text_title;
};

std::vector<TextAttachmentTextTitleTestData> GetTestData() {
  static std::vector<
      TextAttachmentTextTitleTestData>* kTextAttachmentTextTitleTestData =
      new std::vector<TextAttachmentTextTitleTestData>({
          {service::proto::TextMetadata::TEXT, "Short text", "Short text"},
          {service::proto::TextMetadata::TEXT,
           "Long text that should be truncated",
           "Long text that should be truncat…"},
          {service::proto::TextMetadata::URL,
           "https://www.google.com/maps/search/restaurants/@/"
           "data=!3m1!4b1?disco_ad=1234",
           "www.google.com"},
          {service::proto::TextMetadata::URL, "Invalid URL", "Invalid URL"},
          {service::proto::TextMetadata::PHONE_NUMBER, "1234", "xxxx"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+1234", "+xxxx"},
          {service::proto::TextMetadata::PHONE_NUMBER, "123456", "12xxxx"},
          {service::proto::TextMetadata::PHONE_NUMBER, "12345678", "12xxxx78"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+447123456789",
           "+44xxxxxx6789"},
          {service::proto::TextMetadata::PHONE_NUMBER,
           "+1255555555555555555555555555555555555555555555555556789",
           "+12xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx6789"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+16196784004",
           "+16xxxxx4004"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+3841235782564",
           "+38xxxxxxx2564"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+12345678901",
           "+12xxxxx8901"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+1234567891",
           "+12xxxx7891"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+123456789",
           "+12xxxx789"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+12345678",
           "+12xxxx78"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+1234567", "+12xxxx7"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+123456", "+12xxxx"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+12345", "+1xxxx"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+1234", "+xxxx"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+123", "+xxx"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+12", "+xx"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+1", "+x"},
          {service::proto::TextMetadata::PHONE_NUMBER,
           "1255555555555555555555555555555555555555555555555556789",
           "12xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx6789"},
          {service::proto::TextMetadata::PHONE_NUMBER, "12345678901",
           "12xxxxx8901"},
          {service::proto::TextMetadata::PHONE_NUMBER, "1234567891",
           "12xxxx7891"},
          {service::proto::TextMetadata::PHONE_NUMBER, "123456789",
           "12xxxx789"},
          {service::proto::TextMetadata::PHONE_NUMBER, "12345678", "12xxxx78"},
          {service::proto::TextMetadata::PHONE_NUMBER, "1234567", "12xxxx7"},
          {service::proto::TextMetadata::PHONE_NUMBER, "123456", "12xxxx"},
          {service::proto::TextMetadata::PHONE_NUMBER, "12345", "1xxxx"},
          {service::proto::TextMetadata::PHONE_NUMBER, "1234", "xxxx"},
          {service::proto::TextMetadata::PHONE_NUMBER, "123", "xxx"},
          {service::proto::TextMetadata::PHONE_NUMBER, "12", "xx"},
          {service::proto::TextMetadata::PHONE_NUMBER, "1", "x"},
          {service::proto::TextMetadata::PHONE_NUMBER, "+", "+"},
      });

  return *kTextAttachmentTextTitleTestData;
}

using TextAttachmentTextTitleTest =
    testing::TestWithParam<TextAttachmentTextTitleTestData>;

TEST_P(TextAttachmentTextTitleTest, TextTitleMatches) {
  TextAttachment attachment(GetParam().type, GetParam().text_body,
                            /*text_title=*/std::nullopt,
                            /*mime_type=*/std::nullopt);
  EXPECT_EQ(GetParam().expected_text_title, attachment.text_title());
}

INSTANTIATE_TEST_CASE_P(TextAttachmentTextTitleTest,
                        TextAttachmentTextTitleTest,
                        testing::ValuesIn(GetTestData()));

}  // namespace
}  // namespace sharing
}  // namespace nearby
