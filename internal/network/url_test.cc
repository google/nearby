// Copyright 2021 Google LLC
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
#include "internal/network/url.h"

#include <sstream>

#include "gmock/gmock.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"
#include "gtest/gtest.h"

namespace nearby {
namespace network {
namespace {

TEST(Url, TestCreateUrl) {
  auto url = Url::Create("http://www.google.com");
  ASSERT_TRUE(url.ok());

  EXPECT_EQ(url->GetScheme(), "http");
  EXPECT_EQ(url->GetPort(), 80);
  EXPECT_EQ(url->GetHostName(), "www.google.com");

  url = Url::Create("https://www.google.com");
  ASSERT_TRUE(url.ok());
  EXPECT_EQ(url->GetScheme(), "https");
  EXPECT_EQ(url->GetPort(), 443);

  url = Url::Create("https://www.google.com:8443");
  ASSERT_TRUE(url.ok());
  EXPECT_EQ(url->GetPort(), 8443);
  EXPECT_EQ(url->GetUrlPath(), "https://www.google.com:8443");
}

TEST(Url, TestUrlWithPath) {
  auto url = Url::Create("http://www.google.com/users/user/1234");
  ASSERT_TRUE(url.ok());
  EXPECT_EQ(url->GetPath(), "/users/user/1234");
}

TEST(Url, TestUrlWithQuery) {
  auto url = Url::Create("http://www.google.com/user?name=google&age=45");
  ASSERT_TRUE(url.ok());
  auto query_values = url->GetQueryValues("name");
  EXPECT_THAT(query_values, testing::SizeIs(1));
  EXPECT_EQ(query_values[0], "google");
}

TEST(Url, TestUrlWithOneQuery) {
  auto url = Url::Create("http://www.google.com/user?name=google&");
  ASSERT_TRUE(url.ok());
  auto query_values = url->GetQueryValues("name");
  EXPECT_EQ(1u, query_values.size());
  EXPECT_EQ(query_values[0], "google");
}

TEST(Url, TestUrlWithFragment) {
  auto url =
      Url::Create("http://www.google.com/user?name=google&pos=mtv#hello");
  ASSERT_TRUE(url.ok());
  auto query_values = url->GetQueryValues("pos");
  EXPECT_EQ(1u, query_values.size());
  EXPECT_EQ(query_values[0], "mtv");
  EXPECT_EQ(url->GetUrlPath(),
            "http://www.google.com/user?name=google&pos=mtv#hello");
  url->RemoveQueryParameter("pos");
  EXPECT_EQ(0u, url->GetQueryValues("pos").size());
}

TEST(Url, TestGetUrl) {
  auto url = Url::Create("http://www.google.com/user?name=google&pos=mtv");
  ASSERT_TRUE(url.ok());
  EXPECT_EQ(url->GetUrlPath(),
            "http://www.google.com/user?name=google&pos=mtv");
}

TEST(Url, TestAddQueryString) {
  auto url = Url::Create("http://www.google.com/user?name=google&pos=mtv");
  ASSERT_TRUE(url.ok());
  url->AddQueryParameter("test", "good");
  auto new_url = Url::Create(url->GetUrlPath());
  ASSERT_TRUE(new_url.ok());
  auto query_values = new_url->GetQueryValues("test");
  EXPECT_EQ(1u, query_values.size());
  EXPECT_EQ(query_values[0], "good");
}

TEST(Url, TestAddQueryStringWithEncode) {
  auto url = Url::Create("http://www.google.com");
  ASSERT_TRUE(url.ok());
  url->AddQueryParameter("test", "good & bad");
  EXPECT_EQ(url->GetUrlPath(), "http://www.google.com?test=good%20%26%20bad");
  auto new_url = Url::Create(url->GetUrlPath());
  ASSERT_TRUE(new_url.ok());
  auto query_values = new_url->GetQueryValues("test");
  EXPECT_EQ(1u, query_values.size());
  EXPECT_EQ(query_values[0], "good & bad");
}

TEST(Url, TestInvalidUrl) {
  auto url = Url::Create("http:/www.google.com");
  ASSERT_FALSE(url.ok());
  url = Url::Create("ftp://www.google.com");
  ASSERT_FALSE(url.ok());
  url = Url::Create("::::hjskoiskjk");
  ASSERT_FALSE(url.ok());
}

TEST(Url, TestStreamOutput) {
  std::ostringstream stream;
  auto url = Url::Create("https://www.google.com");
  ASSERT_TRUE(url.ok());
  stream << url.value();
  EXPECT_EQ(stream.str(), "https://www.google.com");
}

TEST(Url, TestCompareUrl) {
  auto url1 = Url::Create("https://www.google.com");
  auto url2 = Url::Create("https://www.google.com/");
  ASSERT_TRUE(url1.ok());
  ASSERT_TRUE(url2.ok());
  ASSERT_EQ(url1, url2);
  url1 = Url::Create("https://www.google.com/home?name=test&age=38");
  ASSERT_NE(url1, url2);
  url2 = Url::Create("https://www.google.com/home?age=38&name=test");
  ASSERT_EQ(url1, url2);
  url2 = Url::Create("https://www.google.com/home?age=38&name=test&grade=5");
  ASSERT_NE(url1, url2);
  url2 = Url::Create("https://www.google.com/home?age=38&name=test#fragment");
  ASSERT_NE(url1, url2);
  url2 = Url::Create("https://www.google.com/home?age1=38&name1=test");
  ASSERT_NE(url1, url2);
  url2 = Url::Create("http://www.google.com/home?age1=38&name1=test");
  ASSERT_NE(url1, url2);
  url2 = Url::Create("https://www.google1.com/home?age1=38&name1=test");
  ASSERT_NE(url1, url2);
  url2 = Url::Create("https://www.google.com:8433/home?age=38&name=test");
  ASSERT_NE(url1, url2);
  url2 = Url::Create("https://www.google.com/home?age=38&age=38");
  ASSERT_NE(url2, url1);
  url2 = Url::Create("https://www.google.com/home?age=38&name=test&kk&=888");
  ASSERT_EQ(url1, url2);
}

}  // namespace
}  // namespace network
}  // namespace nearby
