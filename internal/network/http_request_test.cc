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

#include "internal/network/http_request.h"

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"
#include "internal/network/http_body.h"

namespace nearby {
namespace network {
namespace {

using ::testing::SizeIs;

TEST(HttpRequest, TestBuildRequest) {
  auto url = Url::Create("http://www.google.com");
  ASSERT_TRUE(url.ok());

  HttpRequest request_explicit(*url);
  EXPECT_EQ(request_explicit.GetMethodString(), "GET");
  EXPECT_EQ(request_explicit.GetUrl(), *url);

  HttpRequest request;
  EXPECT_EQ(request.GetMethodString(), "GET");
  request.SetUrl(*url);
  EXPECT_EQ(request.GetUrl(), *url);
  request.AddQueryParameter("name", "google");
  EXPECT_THAT(request.GetAllQueryParameters(), SizeIs(1));
  request.AddQueryParameter("name", "android");
  EXPECT_THAT(request.GetAllQueryParameters(), SizeIs(2));
  request.AddHeader("content-type", "text/html");
  EXPECT_THAT(request.GetAllHeaders(), SizeIs(1));
  request.SetMethod(HttpRequestMethod::kPost);
  EXPECT_EQ(request.GetMethodString(), "POST");
  EXPECT_EQ(request.GetMethod(), HttpRequestMethod::kPost);
  request.RemoveQueryParameter("name");
  EXPECT_THAT(request.GetAllQueryParameters(), SizeIs(0));
  request.RemoveHeader("content-type");
  EXPECT_THAT(request.GetAllHeaders(), SizeIs(0));
  request.SetBody("test body");
  EXPECT_EQ(request.GetBody().GetRawData(), "test body");
  HttpRequestBody body;
  body.SetData("new body");
  request.SetBody(body);
  EXPECT_EQ(request.GetBody().GetRawData(), "new body");
  body.SetData(nullptr, 100);
  request.SetBody(body);
  EXPECT_TRUE(request.GetBody().GetRawData().empty());
}

TEST(HttpRequest, TestGetMethodString) {
  HttpRequest request;
  request.SetMethod(HttpRequestMethod::kPost);
  EXPECT_EQ(request.GetMethodString(), "POST");
  request.SetMethod(HttpRequestMethod::kPut);
  EXPECT_EQ(request.GetMethodString(), "PUT");
  request.SetMethod(HttpRequestMethod::kDelete);
  EXPECT_EQ(request.GetMethodString(), "DELETE");
  request.SetMethod(HttpRequestMethod::kConnect);
  EXPECT_EQ(request.GetMethodString(), "CONNECT");
  request.SetMethod(HttpRequestMethod::kHead);
  EXPECT_EQ(request.GetMethodString(), "HEAD");
  request.SetMethod(HttpRequestMethod::kOptions);
  EXPECT_EQ(request.GetMethodString(), "OPTIONS");
  request.SetMethod(HttpRequestMethod::kPatch);
  EXPECT_EQ(request.GetMethodString(), "PATCH");
  request.SetMethod(HttpRequestMethod::kTrace);
  EXPECT_EQ(request.GetMethodString(), "TRACE");
}

}  // namespace
}  // namespace network
}  // namespace nearby
