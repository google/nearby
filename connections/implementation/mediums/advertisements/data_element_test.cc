// Copyright 2025 Google LLC
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

#include "connections/implementation/mediums/advertisements/data_element.h"

#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/stream_reader.h"
#include "internal/platform/stream_writer.h"

namespace nearby::connections::advertisements {
namespace {

TEST(DataElementTest, From) {
  std::optional<DataElement> data_element =
      DataElement::From(0x01, "test_value");
  ASSERT_TRUE(data_element.has_value());
  EXPECT_EQ(data_element->type(), 0x01);
  EXPECT_EQ(data_element->length(), 10);
  EXPECT_EQ(data_element->value(), "test_value");
}

TEST(DataElementTest, FromInvalidType) {
  std::optional<DataElement> data_element =
      DataElement::From(0x00, "test_value");
  EXPECT_FALSE(data_element.has_value());
}

TEST(DataElementTest, FromInvalidLength) {
  std::optional<DataElement> data_element =
      DataElement::From(0x01, std::string(128, 'a'));
  EXPECT_FALSE(data_element.has_value());
}

TEST(DataElementTest, OneByteFromStreamReader) {
  ByteArray bytes("\x21\x01\x02");
  StreamReader reader(bytes);
  std::optional<DataElement> data_element =
      DataElement::FromStreamReader(reader);
  ASSERT_TRUE(data_element.has_value());
  EXPECT_EQ(data_element->type(), 1);
  EXPECT_EQ(data_element->length(), 2);
  EXPECT_EQ(data_element->value(), "\x01\x02");
}

TEST(DataElementTest, TwoByteFromStreamReader) {
  ByteArray bytes("\x84\x01\x01\x02\x03\x04");
  StreamReader reader(bytes);
  std::optional<DataElement> data_element =
      DataElement::FromStreamReader(reader);
  ASSERT_TRUE(data_element.has_value());
  EXPECT_EQ(data_element->type(), 1);
  EXPECT_EQ(data_element->length(), 4);
  EXPECT_EQ(data_element->value(), "\x01\x02\x03\x04");
}

TEST(DataElementTest, FromData) {
  std::string data = "\x21\x01\x02";
  std::optional<DataElement> data_element = DataElement::FromData(data);
  ASSERT_TRUE(data_element.has_value());
  EXPECT_EQ(data_element->type(), 1);
  EXPECT_EQ(data_element->length(), 2);
  EXPECT_EQ(data_element->value(), "\x01\x02");
}

TEST(DataElementTest, InvalidData) {
  EXPECT_FALSE(DataElement::FromData("").has_value());
  EXPECT_FALSE(DataElement::FromData("\x20").has_value());
  EXPECT_FALSE(DataElement::FromData("\x21\x00").has_value());
  EXPECT_FALSE(DataElement::FromData("\x91\x00").has_value());
  EXPECT_FALSE(DataElement::FromData("\x91\x01").has_value());
  EXPECT_TRUE(DataElement::FromData("\x81\x01\x03").has_value());
}

TEST(DataElementTest, ToData) {
  std::string data = "\x21\x01\x03";
  EXPECT_EQ(DataElement::FromData(data)->ToData(), data);
  data = "\x81\x23\x21";
  EXPECT_EQ(DataElement::FromData(data)->ToData(), data);
}

TEST(DataElementTest, ToStreamWriter) {
  StreamWriter writer;
  std::string data = "\x21\x01\x03";
  DataElement::FromData(data)->ToStreamWriter(writer);
  EXPECT_EQ(writer.GetData(), data);
}

}  // namespace
}  // namespace nearby::connections::advertisements
