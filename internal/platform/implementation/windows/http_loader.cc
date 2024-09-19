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

#include "internal/platform/implementation/windows/http_loader.h"

#include <iostream>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/platform/implementation/http_loader.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {
namespace {

constexpr DWORD kSchemaMaximumLength = 10;
constexpr DWORD kHostNameMaximumLength = 256;

using ::nearby::api::WebResponse;

}  // namespace

absl::StatusOr<WebResponse> HttpLoader::GetResponse() {
  absl::Status status;

  status = ParseUrl();
  if (!status.ok()) {
    return status;
  }

  status = ConnectWebServer();
  if (!status.ok()) {
    return status;
  }

  // Sends request to web server.
  status = SendRequest();
  if (!status.ok()) {
    return status;
  }

  // Processes response from web server
  absl::StatusOr<WebResponse> result = ProcessResponse();
  if (!result.ok()) {
    return result;
  }

  DisconnectWebServer();
  return result;
}

absl::StatusOr<int> HttpLoader::QueryStatusCode(HINTERNET file_handle) {
  int result;
  std::string status_code;
  absl::Status status;
  status = QueryResponseInfo(file_handle, HTTP_QUERY_STATUS_CODE, status_code);
  if (!status.ok()) {
    return status;
  }

  if (!absl::SimpleAtoi(
          absl::string_view(status_code.data(), status_code.size() - 1),
          &result)) {
    return absl::InternalError("Failed to parse status code.");
  }

  if (result < 0) {
    return absl::InternalError("Invalid status code.");
  }

  return result;
}

absl::StatusOr<std::string> HttpLoader::QueryStatusText(
    HINTERNET request_handle) {
  absl::Status status;
  std::string status_text;

  status =
      QueryResponseInfo(request_handle, HTTP_QUERY_STATUS_TEXT, status_text);
  if (!status.ok()) {
    return status;
  }

  // status_text is ended with char '\0', remove it in returned result.
  return status_text.substr(0, status_text.size() - 1);
}

absl::StatusOr<std::multimap<std::string, std::string>>
HttpLoader::QueryResponseHeaders(HINTERNET request_handle) {
  absl::Status status;
  std::string headers_string;

  status = QueryResponseInfo(request_handle, HTTP_QUERY_RAW_HEADERS_CRLF,
                             headers_string);
  if (!status.ok()) {
    return status;
  }

  std::multimap<std::string, std::string> headers;
  // Parse headers in response
  size_t start = 0;
  size_t pos = 0;
  while ((pos = headers_string.find("\r\n", start)) != std::string::npos) {
    std::string header = headers_string.substr(start, pos - start);
    // Get key and value in header
    size_t split_pos = 0;
    if ((split_pos = header.find(": ")) != std::string::npos) {
      std::string key = header.substr(0, split_pos);
      std::string value = header.substr(split_pos + 2);
      headers.emplace(key, value);
    }

    start = pos + 2;
  }

  return headers;
}

absl::Status HttpLoader::QueryResponseInfo(HINTERNET request_handle,
                                           DWORD info_level,
                                           std::string& info) {
  DWORD buffer_size = 0;
  BOOL query_result = HttpQueryInfoA(request_handle, info_level, nullptr,
                                     &buffer_size, nullptr);

  if (query_result) {
    return absl::OkStatus();
  }

  if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    info.resize(buffer_size);
    query_result = HttpQueryInfoA(request_handle, info_level, info.data(),
                                  &buffer_size, nullptr);

    if (query_result) {
      return absl::OkStatus();
    }
  }

  return absl::InvalidArgumentError("Failed to query HTTP information.");
}

absl::Status HttpLoader::ParseUrl() {
  const std::string& url = request_.url;
  URL_COMPONENTSA url_components;
  char schema[kSchemaMaximumLength];
  char host_name[kHostNameMaximumLength];
  std::string url_path;
  url_path.resize(url.size());

  memset(&url_components, 0, sizeof(URL_COMPONENTSA));
  url_components.dwStructSize = sizeof(URL_COMPONENTSA);
  url_components.lpszScheme = schema;
  url_components.dwSchemeLength = kSchemaMaximumLength;
  url_components.lpszHostName = host_name;
  url_components.dwHostNameLength = kHostNameMaximumLength;
  url_components.lpszUrlPath = url_path.data();
  url_components.dwUrlPathLength = url.size();

  if (!InternetCrackUrlA(url.data(), url.size(), 0, &url_components)) {
    return absl::InvalidArgumentError("Invalid URL.");
  }

  schema_.assign(url_components.lpszScheme, url_components.dwSchemeLength);

  if (!(schema_ == "http" || schema_ == "https")) {
    return absl::InvalidArgumentError("URL supports HTTP and HTTPS only.");
  }

  if (schema_ == "https") {
    is_secure_ = true;
  }

  host_.assign(url_components.lpszHostName, url_components.dwHostNameLength);
  path_.assign(url_components.lpszUrlPath, url_components.dwUrlPathLength);
  port_ = url_components.nPort;

  return absl::OkStatus();
}

absl::Status HttpLoader::ConnectWebServer() {
  internet_handle_ = InternetOpenA("Mozilla/5.0",                /*Agent*/
                                   INTERNET_OPEN_TYPE_PRECONFIG, /*Access Type*/
                                   nullptr,                      /*Proxy*/
                                   nullptr, /*Proxy bypass*/
                                   0);      /*Flags*/

  if (internet_handle_ == nullptr) {
    LOG(ERROR) << "Failed to open internet with error " << GetLastError()
               << ".";
    return absl::FailedPreconditionError(absl::StrCat(GetLastError()));
  }

  connect_handle_ = InternetConnectA(internet_handle_,      /*Internet*/
                                     host_.c_str(),         /*Server name*/
                                     port_,                 /*Port*/
                                     nullptr,               /*User name*/
                                     nullptr,               /*Password*/
                                     INTERNET_SERVICE_HTTP, /*Service*/
                                     0,                     /*Flags*/
                                     0);                    /*Context*/

  if (connect_handle_ == nullptr) {
    LOG(ERROR) << "Failed to connect remote web server with error "
               << GetLastError() << ".";
    InternetCloseHandle(internet_handle_);
    return absl::FailedPreconditionError(absl::StrCat(GetLastError()));
  }

  return absl::OkStatus();
}

absl::Status HttpLoader::SendRequest() {
  DWORD flags = INTERNET_FLAG_NO_AUTO_REDIRECT;
  if (is_secure_) {
    flags |= INTERNET_FLAG_SECURE;
  }

  request_handle_ =
      HttpOpenRequestA(connect_handle_, request_.method.c_str(), /*Method*/
                       path_.c_str(),                            /*Path*/
                       nullptr, /*HTTP version*/
                       nullptr, /*Referrer*/
                       nullptr, /*Accept types*/
                       flags,   /*Internet options*/
                       0);

  if (request_handle_ == nullptr) {
    LOG(ERROR) << "Failed to open request to remote web server with error "
               << GetLastError() << ".";
    InternetCloseHandle(internet_handle_);
    InternetCloseHandle(connect_handle_);

    return absl::FailedPreconditionError(absl::StrCat(GetLastError()));
  }

  // Prepare headers
  LPCSTR headers_ptr = nullptr;
  DWORD headers_size = 0;
  std::string request_headers;
  for (const auto& header : request_.headers) {
    if (header.second.empty()) {
      continue;
    }

    request_headers.append(header.first + ": " + header.second + "\r\n");
  }

  if (!request_headers.empty()) {
    headers_ptr = request_headers.data();
    headers_size = request_headers.size();
  }

  LPVOID data_ptr = nullptr;
  DWORD data_size = 0;

  // Prepare request data
  if (!request_.body.empty()) {
    data_ptr = (LPVOID)(request_.body.data());
    data_size = request_.body.size();
  }

  BOOL result = HttpSendRequestA(request_handle_, /*Request*/
                                 headers_ptr,     /*Headers*/
                                 headers_size,    /*Header size*/
                                 data_ptr,        /*Data*/
                                 data_size);      /*Data size*/

  if (result == FALSE) {
    LOG(ERROR) << "Failed to send request to remote web server with error "
               << GetLastError() << ".";
    InternetCloseHandle(request_handle_);
    InternetCloseHandle(connect_handle_);
    InternetCloseHandle(internet_handle_);

    return absl::FailedPreconditionError(absl::StrCat(GetLastError()));
  }

  return absl::OkStatus();
}

absl::StatusOr<WebResponse> HttpLoader::ProcessResponse() {
  absl::Status status;
  WebResponse web_response;
  auto status_code = QueryStatusCode(request_handle_);
  if (!status_code.ok()) {
    return absl::InternalError("Failed to read HTTP status");
  }

  web_response.status_code = status_code.value();
  auto status_text = QueryStatusText(request_handle_);
  if (!status_text.ok()) {
    return absl::InternalError("Failed to read HTTP status");
  }

  web_response.status_text = status_text.value();
  auto headers = QueryResponseHeaders(request_handle_);
  if (!headers.ok()) {
    headers.status();
  }
  web_response.headers = *headers;

  // Get response data
  char buffer[kReceiveBufferSize];

  while (true) {
    DWORD read_size;
    BOOL read_result = InternetReadFile(request_handle_, buffer,
                                        kReceiveBufferSize, &read_size);

    if (read_result) {
      if (read_size == 0) {
        break;
      } else {
        // Append data to response
        web_response.body.append(buffer, read_size);
      }
    } else {
      LOG(ERROR) << "Failed to read response from remote web server with error "
                 << GetLastError() << ".";
      InternetCloseHandle(request_handle_);
      InternetCloseHandle(connect_handle_);
      InternetCloseHandle(internet_handle_);
      return absl::FailedPreconditionError(absl::StrCat(GetLastError()));
    }
  }

  status = HTTPCodeToStatus(web_response.status_code, web_response.status_text);
  if (!status.ok()) {
    return status;
  }

  return web_response;
}

void HttpLoader::DisconnectWebServer() {
  if (request_handle_ != nullptr) {
    InternetCloseHandle(request_handle_);
    request_handle_ = nullptr;
  }

  if (connect_handle_ != nullptr) {
    InternetCloseHandle(connect_handle_);
    connect_handle_ = nullptr;
  }

  if (internet_handle_ != nullptr) {
    InternetCloseHandle(internet_handle_);
    internet_handle_ = nullptr;
  }
}

absl::Status HttpLoader::HTTPCodeToStatus(int status_code,
                                          absl::string_view status_message) {
  switch (status_code) {
    case 400:
      return absl::InvalidArgumentError(status_message);
    case 401:
      return absl::UnauthenticatedError(status_message);
    case 403:
      return absl::PermissionDeniedError(status_message);
    case 404:
      return absl::NotFoundError(status_message);
    case 409:
      return absl::AbortedError(status_message);
    case 416:
      return absl::OutOfRangeError(status_message);
    case 429:
      return absl::ResourceExhaustedError(status_message);
    case 499:
      return absl::CancelledError(status_message);
    case 504:
      return absl::DeadlineExceededError(status_message);
    case 501:
      return absl::UnimplementedError(status_message);
    case 503:
      return absl::UnavailableError(status_message);
    default:
      break;
  }
  if (status_code >= 200 && status_code < 300) {
    return absl::OkStatus();
  } else if (status_code >= 400 && status_code < 500) {
    return absl::FailedPreconditionError(status_message);
  } else if (status_code >= 500 && status_code < 600) {
    return absl::InternalError(status_message);
  }
  return absl::UnknownError(status_message);
}

}  // namespace windows
}  // namespace nearby
