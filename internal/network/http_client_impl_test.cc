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

#include "internal/network/http_client_impl.h"

#include <cstddef>
#include <map>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "internal/network/http_status_code.h"
#include "internal/platform/implementation/http_loader.h"
#include "internal/platform/implementation/platform.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"

namespace nearby {
namespace api {
namespace {

struct HttpTestContext {
  WebRequest web_request;
  WebResponse web_response;
  absl::Status status;
};

HttpTestContext* GetContext() {
  static HttpTestContext* context = new HttpTestContext();
  return context;
}

}  // namespace

// Mock web implementation of the platform
absl::StatusOr<WebResponse> ImplementationPlatform::SendRequest(
    const WebRequest& request) {
  GetContext()->web_request = request;
  if (GetContext()->status.ok()) {
    return GetContext()->web_response;
  }
  return GetContext()->status;
}

}  // namespace api

namespace network {

using ::testing::SizeIs;

class NearbyHttpClientTest : public ::testing::Test {
 public:
  void SetUp() override {
    api::GetContext()->web_request = api::WebRequest();
    api::GetContext()->web_response = api::WebResponse();
    api::GetContext()->status = absl::Status();
  }

  void MockFailedResponse(absl::Status status) {
    api::GetContext()->status = status;
  }

  void MockResponse(network::HttpStatusCode status,
                    absl::string_view status_message,
                    const std::multimap<std::string, std::string>& headers,
                    absl::string_view body) {
    api::WebResponse web_response;
    web_response.status_code = static_cast<int>(status);
    web_response.status_text = absl::StrCat(status_message);
    web_response.headers = headers;
    web_response.body = absl::StrCat(body);
    api::GetContext()->web_response = web_response;
  }

  api::WebRequest GetWebRequest() { return api::GetContext()->web_request; }

  absl::StatusOr<HttpResponse> GetResponse(
      absl::string_view url, HttpRequestMethod method,
      const std::multimap<std::string, std::string>& headers,
      absl::string_view body) {
    absl::StatusOr<HttpResponse> result;
    absl::StatusOr<Url> request_url = Url::Create(url);
    if (!request_url.ok()) {
      return request_url.status();
    }

    HttpRequest request{request_url.value()};
    auto it = headers.begin();
    while (it != headers.end()) {
      request.AddHeader(it->first, it->second);
      ++it;
    }
    request.SetMethod(method);
    request.SetBody(body);

    absl::Notification notification;
    client_.StartRequest(
        request, [&result, &notification](
                     const absl::StatusOr<HttpResponse>& http_response) {
          result = http_response;
          notification.Notify();
        });

    // No timeout to emulate original behavior
    notification.WaitForNotification();
    return result;
  }

  void CheckHeader(const std::multimap<std::string, std::string>& headers,
                   absl::string_view key, absl::string_view expected_value) {
    auto it = headers.find(std::string(key));
    ASSERT_TRUE(it != headers.end());

    bool found = false;
    while (it != headers.end()) {
      if (it->second == absl::StrCat(expected_value)) {
        found = true;
        break;
      }
      ++it;
    }

    EXPECT_TRUE(found);
  }

 private:
  NearbyHttpClient client_;
};

namespace {
TEST_F(NearbyHttpClientTest, TestGet) {
  MockResponse(HttpStatusCode::kHttpOk, "OK", {{"Content_Type", "text/html"}},
               "web content");
  auto result =
      GetResponse("http://www.google.com", HttpRequestMethod::kGet, {}, "");

  // Checks request.
  api::WebRequest web_request = GetWebRequest();
  EXPECT_EQ(web_request.url, "http://www.google.com");
  EXPECT_EQ(web_request.method, "GET");

  // Checks response.
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->GetStatusCode(), HttpStatusCode::kHttpOk);
  EXPECT_EQ(result->GetBody().GetRawData(), "web content");
  ASSERT_THAT(result->GetAllHeaders(), SizeIs(1));
  EXPECT_EQ(result->GetAllHeaders().at("Content_Type")[0], "text/html");
}

TEST_F(NearbyHttpClientTest, TestGetWithQuery) {
  MockResponse(HttpStatusCode::kHttpOk, "OK", {{"Content_Type", "text/html"}},
               "web content");
  auto result = GetResponse("http://www.google.com?name=name1&age=36",
                            HttpRequestMethod::kGet, {}, "");

  // Checks request.
  api::WebRequest web_request = GetWebRequest();
  EXPECT_EQ(web_request.url, "http://www.google.com?name=name1&age=36");
  EXPECT_EQ(web_request.method, "GET");

  // Checks response.
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->GetStatusCode(), HttpStatusCode::kHttpOk);
}

TEST_F(NearbyHttpClientTest, TestGetWithErrorResult) {
  MockFailedResponse(absl::InternalError("no connection."));
  auto result = GetResponse("http://www.google.com?name=name1&age=36",
                            HttpRequestMethod::kGet, {}, "");

  // Checks request.
  api::WebRequest web_request = GetWebRequest();
  EXPECT_EQ(web_request.url, "http://www.google.com?name=name1&age=36");
  EXPECT_EQ(web_request.method, "GET");

  // Checks response.
  EXPECT_FALSE(result.ok());
}

TEST_F(NearbyHttpClientTest, TestPost) {
  MockResponse(HttpStatusCode::kHttpNoContent, "OK",
               {{"Content_Type", "text/html"}}, "");
  auto result =
      GetResponse("http://www.google.com", HttpRequestMethod::kPost, {}, "");

  // Checks request.
  api::WebRequest web_request = GetWebRequest();
  EXPECT_EQ(web_request.url, "http://www.google.com");
  EXPECT_EQ(web_request.method, "POST");

  // Checks response.
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->GetStatusCode(), HttpStatusCode::kHttpNoContent);
  HttpResponseBody body = result->GetBody();
  EXPECT_TRUE(body.empty());
}

TEST_F(NearbyHttpClientTest, TestPostWithHeader) {
  MockResponse(HttpStatusCode::kHttpNoContent, "OK",
               {{"Content_Type", "text/html"}}, "");
  auto result =
      GetResponse("http://www.google.com", HttpRequestMethod::kPost,
                  {{"Content_Type", "text/json"}, {"size", "596"}}, "");

  // Checks request.
  api::WebRequest web_request = GetWebRequest();
  EXPECT_EQ(web_request.url, "http://www.google.com");
  EXPECT_EQ(web_request.method, "POST");
  ASSERT_NO_FATAL_FAILURE(
      CheckHeader(web_request.headers, "Content_Type", "text/json"));
  ASSERT_NO_FATAL_FAILURE(CheckHeader(web_request.headers, "size", "596"));

  // Checks response.
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->GetStatusCode(), HttpStatusCode::kHttpNoContent);
  EXPECT_EQ(result->GetBody().GetRawData(), "");
}

TEST_F(NearbyHttpClientTest, TestPostWithErrorResult) {
  MockFailedResponse(absl::UnauthenticatedError("no user."));
  auto result =
      GetResponse("http://www.google.com", HttpRequestMethod::kPost,
                  {{"Content_Type", "text/json"}, {"size", "596"}}, "");

  // Checks request.
  api::WebRequest web_request = GetWebRequest();
  EXPECT_EQ(web_request.url, "http://www.google.com");
  EXPECT_EQ(web_request.method, "POST");
  ASSERT_NO_FATAL_FAILURE(
      CheckHeader(web_request.headers, "Content_Type", "text/json"));
  ASSERT_NO_FATAL_FAILURE(CheckHeader(web_request.headers, "size", "596"));

  // Checks response.
  ASSERT_FALSE(result.ok());
}

TEST_F(NearbyHttpClientTest, TestRequestWithCleanThreads) {
  MockResponse(HttpStatusCode::kHttpOk, "OK", {{"Content_Type", "text/html"}},
               "web content");
  auto result =
      GetResponse("http://www.google.com", HttpRequestMethod::kGet, {}, "");

  // Checks request.
  api::WebRequest web_request = GetWebRequest();
  EXPECT_EQ(web_request.url, "http://www.google.com");
  EXPECT_EQ(web_request.method, "GET");

  // Checks response.
  ASSERT_TRUE(result.ok());

  result =
      GetResponse("http://www.youtube.com", HttpRequestMethod::kGet, {}, "");
  ASSERT_TRUE(result.ok());
}

}  // namespace
}  // namespace network
}  // namespace nearby
