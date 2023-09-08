// Copyright 2020 Google LLC
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

#include <string>

#include "absl/strings/ascii.h"
#include "internal/platform/implementation/linux/utils.h"

#include "gtest/gtest.h"

namespace nearby {
namespace linux {

TEST(UtilsTests, UuidFromStringRoundTrip) {
  std::string input = "b5209043-f493-4b38-8c34-810aa3cd1407";

  auto nearby_uuid = UuidFromString(input);
  EXPECT_TRUE(nearby_uuid.has_value());


  EXPECT_EQ(absl::AsciiStrToLower(std::string{*nearby_uuid}),
            "b5209043-f493-4b38-8c34-810aa3cd1407");
}
}  // namespace linux
}  // namespace nearby
