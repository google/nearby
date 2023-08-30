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

#include "internal/platform/implementation/linux/http_loader.h"

#include <iostream>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace linux {
namespace {

constexpr int32_t kSchemaMaximumLength = 10;
constexpr int32_t kHostNameMaximumLength = 256;

using ::nearby::api::WebResponse;

}  // namespace

HttpLoader::HttpLoader(const nearby::api::WebRequest &request)
    : request_(request),
      header_data_(open_memstream(&header_strings_, &header_sizeloc_)),
      our_header_data_(nullptr),
      response_data_(open_memstream(&response_strings_, &response_sizeloc_)),
      curl_(curl_easy_init()) {}

HttpLoader::~HttpLoader() { DisconnectWebServer(); }

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

absl::StatusOr<int> HttpLoader::QueryStatusCode(CURL *file_handle) {
  long status_code;
  absl::Status status;
  status = QueryResponseInfo(file_handle, CURLINFO_RESPONSE_CODE, &status_code);
  if (!status.ok()) {
    return status;
  }

  if (status_code < 0) {
    return absl::InternalError("Invalid status code.");
  }

  return status_code;
}

absl::StatusOr<std::string> HttpLoader::QueryStatusText(CURL *request_handle) {
  absl::StatusOr<int> status;
  std::string status_text;

  status = QueryStatusCode(request_handle);
  if (!status.ok()) {
    return status.status();
  }

  switch (status.value()) {
    case 100:
      return "Continue";
    case 101:
      return "Switching Protocols";
    case 102:
      return "Processing";
    case 103:
      return "Early Hints";
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 202:
      return "Accepted";
    case 203:
      return "Non-Authoritative Information";
    case 204:
      return "No Content";
    case 205:
      return "Reset Content";
    case 206:
      return "Partial Content";
    case 207:
      return "Multi-Status";
    case 208:
      return "Already Reported";
    case 226:
      return "IM Used";
    case 300:
      return "Multiple Choices";
    case 301:
      return "Moved Permanently";
    case 302:
      return "Found";
    case 303:
      return "See Other";
    case 304:
      return "Not Modified";
    case 305:
      return "Use Proxy";
    case 307:
      return "Temporary Redirect";
    case 308:
      return "Permanent Redirect";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 402:
      return "Payment Required";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 406:
      return "Not Acceptable";
    case 407:
      return "Proxy Authentication Required";
    case 408:
      return "Request Timeout";
    case 409:
      return "Conflict";
    case 410:
      return "Gone";
    case 411:
      return "Lenth Required";
    case 412:
      return "Precondition Failed";
    case 413:
      return "Payload Too Large";
    case 414:
      return "URI Too Long";
    case 415:
      return "Unsupported Media Type";
    case 416:
      return "Range Not Satisfiable";
    case 417:
      return "Expectation Failed";
    case 418:
      return "I'm a teapot!";
    case 421:
      return "Misdirected Request";
    case 422:
      return "Unprocessable Content";
    case 423:
      return "Locked";
    case 424:
      return "Failed Dependency";
    case 425:
      return "Too Early";
    case 426:
      return "Upgrade Required";
    case 428:
      return "Precondition Required";
    case 429:
      return "Too Many Requests";
    case 431:
      return "Request Header Fields Too Large";
    case 451:
      return "Unavailable For Legal Reasons";
    case 500:
      return "Internal Server Error";
    case 501:
      return "Not Implemented";
    case 502:
      return "Bad Gateway";
    case 503:
      return "Service Unavailable";
    case 504:
      return "Gateway Timeout";
    case 505:
      return "HTTP Version Not Supported";
    case 506:
      return "Variant Also Negotiates";
    case 507:
      return "Insufficient Storage";
    case 508:
      return "Loop Detected";
    case 509:
      return "Network Authentication Required";
    default:
      return absl::InternalError("Invalid status code.");
  }
}

absl::StatusOr<std::multimap<std::string, std::string>>
HttpLoader::QueryResponseHeaders(CURL *request_handle) {
  absl::Status status;
  long header_size;

  status = QueryResponseInfo(curl_, CURLINFO_HEADER_SIZE, &header_size);

  if (!status.ok()) {
    return status;
  }

  std::string headers_string(header_strings_, header_size);

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

const nearby::api::WebRequest &HttpLoader::GetRequest() { return request_; }

size_t HttpLoader::CurlReadCallback(char *buffer, size_t size, size_t nitems,
                                    void *userdata) {
  size_t write_size_max = size * nitems;
  size_t write_amount = 0;
  for (const auto &str :
       reinterpret_cast<HttpLoader *>(userdata)->GetRequest().body) {
    if (write_amount == write_size_max) {
      break;
    }
    *(buffer + write_amount) = str;
    write_amount++;
  }

  return write_amount;
}

// This function uses the CURL getinfo function to grab info. Each info_level
// has a different type it can return. It would not be feasable to determine the
// type and return it. IT IS UP TO THE CALLER OF THE FUNCTION TO USE THE void*
// CORRECTLY.
absl::Status HttpLoader::QueryResponseInfo(CURL *request_handle,
                                           CURLINFO info_level, void *info) {
  CURLcode query_result = curl_easy_getinfo(request_handle, info_level, &info);

  if (query_result == CURLE_OK) {
    return absl::OkStatus();
  }

  return absl::InvalidArgumentError(
      "Failed to query HTTP information: " +
      std::string(curl_easy_strerror(query_result)));
}

absl::Status HttpLoader::ParseUrl() {
  CURLU *url_components = curl_url();
  char *schema;
  char *host_name;
  char *path;

  CURLUcode ret = curl_url_set(url_components, CURLUPART_URL,
                               request_.url.c_str(), CURLU_NON_SUPPORT_SCHEME);
  if (ret) {
    curl_url_cleanup(url_components);
    curl_free(schema);
    curl_free(host_name);
    curl_free(path);
    url_components = nullptr;
    schema = nullptr;
    host_name = nullptr;
    path = nullptr;
    return absl::InvalidArgumentError("Invalid URL format: " +
                                      std::string(curl_url_strerror(ret)));
  }

  ret = curl_url_get(url_components, CURLUPART_SCHEME, &schema,
                     CURLU_URLDECODE | CURLU_URLENCODE | CURLU_DEFAULT_PORT |
                         CURLU_DEFAULT_SCHEME);
  if (ret) {
    curl_url_cleanup(url_components);
    curl_free(schema);
    curl_free(host_name);
    curl_free(path);
    url_components = nullptr;
    schema = nullptr;
    host_name = nullptr;
    path = nullptr;
    return absl::InvalidArgumentError("Could not parse URL schema: " +
                                      std::string(curl_url_strerror(ret)));
  }

  ret = curl_url_get(url_components, CURLUPART_PATH, &path,
                     CURLU_URLDECODE | CURLU_URLENCODE | CURLU_DEFAULT_PORT |
                         CURLU_DEFAULT_SCHEME);
  if (ret) {
    curl_url_cleanup(url_components);
    curl_free(schema);
    curl_free(host_name);
    curl_free(path);
    url_components = nullptr;
    schema = nullptr;
    host_name = nullptr;
    path = nullptr;
    return absl::InvalidArgumentError("Could not parse URL path: " +
                                      std::string(curl_url_strerror(ret)));
  }

  if (!(schema_ == "http" || schema_ == "https")) {
    curl_url_cleanup(url_components);
    curl_free(schema);
    curl_free(host_name);
    curl_free(path);
    url_components = nullptr;
    schema = nullptr;
    host_name = nullptr;
    path = nullptr;
    return absl::InvalidArgumentError("URL supports HTTP and HTTPS only.");
  }

  host_ = host_name;
  schema_ = schema;
  path_ = path;

  if (schema_ == "https") {
    is_secure_ = true;
  }

  curl_url_cleanup(url_components);
  curl_free(schema);
  curl_free(host_name);
  curl_free(path);
  url_components = nullptr;
  schema = nullptr;
  host_name = nullptr;
  path = nullptr;
  return absl::OkStatus();
}

absl::Status HttpLoader::ConnectWebServer() {
  std::vector<CURLcode> option_return_codes;
  if (curl_) {
    curl_ = curl_easy_init();
    header_data_ = open_memstream(&header_strings_, &header_sizeloc_);
    response_data_ = open_memstream(&response_strings_, &response_sizeloc_);
  }

  option_return_codes.push_back(
      curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 1L));
  option_return_codes.push_back(
      curl_easy_setopt(curl_, CURLOPT_URL, request_.url.c_str()));
  option_return_codes.push_back(curl_easy_setopt(curl_, CURLOPT_PORT, port_));
  option_return_codes.push_back(
      curl_easy_setopt(curl_, CURLOPT_AUTOREFERER, 1L));
  option_return_codes.push_back(
      curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L));
  option_return_codes.push_back(
      curl_easy_setopt(curl_, CURLOPT_USERAGENT, "Mozilla/5.0"));
  option_return_codes.push_back(
      curl_easy_setopt(curl_, CURLOPT_HEADERDATA, header_data_));
  option_return_codes.push_back(
      curl_easy_setopt(curl_, CURLOPT_WRITEDATA, response_data_));

  // Prepare headers
  std::string request_headers;
  for (const auto &header : request_.headers) {
    struct curl_slist *list = curl_slist_append(
        our_header_data_,
        std::string(header.first + ": " + header.second).c_str());
    if (list) {
      our_header_data_ = list;
    }
  }

  if (!request_headers.empty()) {
    option_return_codes.push_back(
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, our_header_data_));
  }

  if (request_.method == "GET") {
    option_return_codes.push_back(curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L));
  } else if (request_.method == "POST") {
    option_return_codes.push_back(curl_easy_setopt(
        curl_, CURLOPT_POSTFIELDSIZE, static_cast<long>(request_.body.size())));
    option_return_codes.push_back(
        curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, request_.body.c_str()));
  } else if (request_.method == "PUT") {
    option_return_codes.push_back(curl_easy_setopt(curl_, CURLOPT_UPLOAD, 1L));
    option_return_codes.push_back(
        curl_easy_setopt(curl_, CURLOPT_READFUNCTION, CurlReadCallback));
    option_return_codes.push_back(
        curl_easy_setopt(curl_, CURLOPT_READDATA, this));
    option_return_codes.push_back(curl_easy_setopt(
        curl_,
        (request_.body.size() < std::numeric_limits<long>::max()
             ? CURLOPT_INFILESIZE
             : CURLOPT_INFILESIZE_LARGE),
        request_.body.size()));

  } else {
    NEARBY_LOGS(ERROR) << "Failed to open internet with error "
                       << "Invalid request method: " << request_.method << ".";
    return absl::FailedPreconditionError(
        "Failed to open internet: Invalid request method.");
  }

  for (const auto &ret : option_return_codes) {
    if (ret) {
      NEARBY_LOGS(ERROR) << "Failed to open internet with error "
                         << curl_easy_strerror(ret) << ".";
      return absl::FailedPreconditionError(
          absl::StrCat(curl_easy_strerror(ret)));
    }
  }

  return absl::OkStatus();
}

absl::Status HttpLoader::SendRequest() {
  CURLcode ret = curl_easy_perform(curl_);

  if (ret != CURLE_OK) {
    NEARBY_LOGS(ERROR)
        << "Failed to send request to remote web server with error "
        << curl_easy_strerror(ret) << ".";
    return absl::FailedPreconditionError(absl::StrCat(curl_easy_strerror(ret)));
  }

  return absl::OkStatus();
}

absl::StatusOr<WebResponse> HttpLoader::ProcessResponse() {
  absl::Status status;
  WebResponse web_response;
  auto status_code = QueryStatusCode(curl_);
  if (!status_code.ok()) {
    return absl::InternalError("Failed to read HTTP status");
  }

  web_response.status_code = status_code.value();
  auto status_text = QueryStatusText(curl_);
  if (!status_text.ok()) {
    return absl::InternalError("Failed to read HTTP status");
  }

  web_response.status_text = status_text.value();
  auto headers = QueryResponseHeaders(curl_);
  if (!headers.ok()) {
    headers.status();
  }
  web_response.headers = *headers;

  curl_off_t download_size;

  CURLcode ret =
      curl_easy_getinfo(curl_, CURLINFO_SIZE_DOWNLOAD_T, &download_size);

  if (ret) {
    if (download_size != 0) {
      // Append data to response
      web_response.body.assign(response_strings_, download_size);
    } else {
      NEARBY_LOGS(ERROR)
          << "Failed to read response from remote web server with error "
          << curl_easy_strerror(ret) << ".";
      return absl::FailedPreconditionError(
          absl::StrCat(curl_easy_strerror(ret)));
    }
  }

  status = HTTPCodeToStatus(web_response.status_code, web_response.status_text);
  if (!status.ok()) {
    return status;
  }

  return web_response;
}

void HttpLoader::DisconnectWebServer() {
  fclose(header_data_);
  header_data_ = nullptr;
  delete header_strings_;
  header_strings_ = nullptr;
  curl_easy_cleanup(curl_);
  curl_ = nullptr;
  curl_slist_free_all(our_header_data_);
  our_header_data_ = nullptr;
  fclose(response_data_);
  response_data_ = nullptr;
  delete response_strings_;
  response_strings_ = nullptr;
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

}  // namespace linux
}  // namespace nearby
