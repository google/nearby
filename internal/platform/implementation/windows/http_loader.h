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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_HTTP_LOADER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_HTTP_LOADER_H_

#include <windows.h>  // NOLINT
#include <wininet.h>

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/http_loader.h"

namespace nearby {
namespace windows {

// HttpLoader is used to get HTTP response from remote server.
//
// HttpLoader gets HTTP request information from caller, and calling Windows
// WinInet APIs to get HTTP response. The platform handles HTTP/HTTPS sessions.
class HttpLoader {
 public:
  explicit HttpLoader(const nearby::api::WebRequest& request)
      : request_(request) {}
  ~HttpLoader() = default;

  absl::StatusOr<nearby::api::WebResponse> GetResponse();

 private:
  // Defines the buffer size. It is used to init a buffer for receiving HTTP
  // response. The unit is byte.
  static constexpr int kReceiveBufferSize = 8 * 1024;

  absl::Status ConnectWebServer();
  absl::Status SendRequest();
  absl::StatusOr<nearby::api::WebResponse> ProcessResponse();
  void DisconnectWebServer();

  absl::StatusOr<int> QueryStatusCode(HINTERNET file_handle);
  absl::StatusOr<std::string> QueryStatusText(HINTERNET request_handle);
  absl::StatusOr<std::multimap<std::string, std::string>> QueryResponseHeaders(
      HINTERNET request_handle);
  absl::Status QueryResponseInfo(HINTERNET request_handle, DWORD info_level,
                                 std::string& info);

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

  HINTERNET internet_handle_ = nullptr;
  HINTERNET connect_handle_ = nullptr;
  HINTERNET request_handle_ = nullptr;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_HTTP_LOADER_H_
