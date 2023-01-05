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

#include "internal/network/http_response.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace network {
namespace {

using ::testing::SizeIs;

TEST(HttpResponse, TestBuildResponse) {
  HttpResponse response;
  response.SetStatusCode(HttpStatusCode::kHttpNotFound);
  EXPECT_EQ(response.GetStatusCode(), HttpStatusCode::kHttpNotFound);
  response.SetReasonPhrase("OK");
  EXPECT_EQ(response.GetReasonPhrase(), "OK");
  response.AddHeader("Content-Type", "text/html");
  EXPECT_THAT(response.GetAllHeaders(), SizeIs(1));
  response.RemoveHeader("Content-Type");
  EXPECT_THAT(response.GetAllHeaders(), SizeIs(0));
  HttpResponseBody body;
  body.SetData("test body");
  response.SetBody(body);
  EXPECT_EQ(response.GetBody().GetRawData(), "test body");
}

}  // namespace
}  // namespace network
}  // namespace nearby
