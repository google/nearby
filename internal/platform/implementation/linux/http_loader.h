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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_HTTP_LOADER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_HTTP_LOADER_H_

#include <curl/curl.h>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/http_loader.h"

namespace nearby {
namespace linux {

// HttpLoader is used to get HTTP response from remote server.
//
// HttpLoader gets HTTP request information from caller, and calling Windows
// WinInet APIs to get HTTP response. The platform handles HTTP/HTTPS sessions.
class HttpLoader {
 public:
  explicit HttpLoader(const nearby::api::WebRequest &request);
  ~HttpLoader();

  absl::StatusOr<nearby::api::WebResponse> GetResponse();

  const nearby::api::WebRequest &GetRequest();

 private:
  // Defines the buffer size. It is used to init a buffer for receiving HTTP
  // response. The unit is byte.
  static constexpr int kReceiveBufferSize = 8 * 1024;
  static size_t CurlReadCallback(char *buffer, size_t size, size_t nitems,
                                 void *userdata);

  absl::Status ConnectWebServer();
  absl::Status SendRequest();
  absl::StatusOr<nearby::api::WebResponse> ProcessResponse();
  void DisconnectWebServer();

  absl::StatusOr<int> QueryStatusCode(CURL *file_handle);
  absl::StatusOr<std::string> QueryStatusText(CURL *request_handle);
  absl::StatusOr<std::multimap<std::string, std::string>> QueryResponseHeaders(
      CURL *request_handle);
  absl::Status QueryResponseInfo(CURL *request_handle, CURLINFO info_level,
                                 void *info);

  absl::Status ParseUrl();

  // Converts HTTP status code to absl Status.
  //
  // @param status_code HTTP status code, such 200, 404 etc.
  // @param status_message short description of the status code.
  // @return converted absl status.
  absl::Status HTTPCodeToStatus(int status_code,
                                absl::string_view status_message);

  nearby::api::WebRequest request_;
  std::string host_;
  std::string path_;
  std::string schema_;
  bool is_secure_ = false;
  int port_ = 80;

  FILE *header_data_;
  char *header_strings_;
  size_t header_sizeloc_;
  struct curl_slist *our_header_data_;
  FILE *response_data_;
  char *response_strings_;
  size_t response_sizeloc_;
  CURL *curl_;
};

}  // namespace linux
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_LINUX_HTTP_LOADER_H_
