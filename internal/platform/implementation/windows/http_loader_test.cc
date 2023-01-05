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

#include "internal/platform/implementation/windows/http_loader.h"

#include <string>

#include "gtest/gtest.h"

namespace nearby {
namespace windows {
namespace {
using ::nearby::api::WebRequest;

TEST(HttpLoader, DISABLED_TestGetUrl) {
  WebRequest request;
  request.url = "https://www.google.com?id=456#fragment";
  request.method = "GET";
  auto response = HttpLoader(request).GetResponse();
  ASSERT_TRUE(response.ok());
  EXPECT_EQ(response->status_code, 200);
}

TEST(HttpLoader, DISABLED_TestGetNotExistingUrl) {
  WebRequest request;
  request.url = "https://www.abcdefgabcdefg.com";
  request.method = "GET";
  EXPECT_FALSE(HttpLoader(request).GetResponse().ok());
}

TEST(HttpLoader, DISABLED_TestInvalidUrl) {
  WebRequest request;
  request.url = "https:/www.abcdefgabcdefg.com/name?id=456";
  request.method = "GET";
  EXPECT_FALSE(HttpLoader(request).GetResponse().ok());
}

}  // namespace
}  // namespace windows
}  // namespace nearby
