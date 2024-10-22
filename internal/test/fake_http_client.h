// Copyright 2021-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_HTTP_CLIENT_H_
#define THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_HTTP_CLIENT_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "internal/network/http_client.h"
#include "internal/network/http_request.h"
#include "internal/network/http_response.h"
#include "internal/network/http_status_code.h"

namespace nearby {
namespace network {

class FakeHttpClient : public HttpClient {
 public:
  struct RequestInfo {
    HttpRequest request;
    absl::AnyInvocable<void(const absl::StatusOr<HttpResponse>&)> callback;
  };

  FakeHttpClient() = default;
  explicit FakeHttpClient(const HttpRequest& request);
  ~FakeHttpClient() override = default;

  FakeHttpClient(const FakeHttpClient&) = default;
  FakeHttpClient& operator=(const FakeHttpClient&) = default;
  FakeHttpClient(FakeHttpClient&&) = default;
  FakeHttpClient& operator=(FakeHttpClient&&) = default;

  void StartRequest(
      const HttpRequest& request,
      absl::AnyInvocable<void(const absl::StatusOr<HttpResponse>&)> callback)
      override {
    RequestInfo request_info;
    request_info.request = request;
    request_info.callback = std::move(callback);
    request_infos_.push_back(std::move(request_info));
  }

  void StartCancellableRequest(
      std::unique_ptr<CancellableRequest> request,
      absl::AnyInvocable<void(const absl::StatusOr<HttpResponse>&)> callback)
      override {
    RequestInfo request_info;
    request_info.request = request->http_request();
    request_info.callback = std::move(callback);
    request_infos_.push_back(std::move(request_info));
  }

  absl::StatusOr<HttpResponse> GetResponse(
      const HttpRequest& request) override {
    if (sync_responses_.empty()) {
      return absl::FailedPreconditionError("No response.");
    }
    auto response = sync_responses_.front();
    sync_responses_.erase(sync_responses_.begin());
    return response;
  }

  void SetResponseForSyncRequest(const absl::StatusOr<HttpResponse>& response) {
    sync_responses_.push_back(response);
  }

  // Mock methods
  void CompleteRequest(const absl::StatusOr<HttpResponse>& response,
                       size_t pos = 0) {
    if (pos >= request_infos_.size()) {
      return;
    }
    auto& request_info = request_infos_.at(pos);
    if (request_info.callback) {
      request_info.callback(response);
    }

    request_infos_.erase(request_infos_.begin() + pos);
  }

  void CompleteRequest(
      int error,
      std::optional<nearby::network::HttpStatusCode> response_code =
          std::nullopt,
      const std::optional<std::string>& response_string = std::nullopt,
      size_t pos = 0) {
    absl::StatusOr<nearby::network::HttpResponse> result;

    nearby::network::HttpResponse response;
    if (error == 0) {
      absl::Status status =
          HTTPCodeToStatus(static_cast<int>(*response_code), "");
      if (status.ok()) {
        if (response_code.has_value()) {
          response.SetStatusCode(*response_code);
        }

        if (response_string.has_value()) {
          response.SetBody(*response_string);
        }

        response.SetHeaders({{"Content-type", {"text/html"}}});
        result = response;
      } else {
        result = status;
      }
    } else {
      result = absl::Status(absl::StatusCode::kFailedPrecondition,
                            std::to_string(error));
    }

    CompleteRequest(result, pos);
  }

  absl::Status HTTPCodeToStatus(int status_code,
                                absl::string_view status_message) {
    absl::Status status = absl::OkStatus();
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

  const std::vector<RequestInfo>& GetPendingRequest() { return request_infos_; }

 private:
  std::vector<RequestInfo> request_infos_;

  // Response for sync request
  std::vector<absl::StatusOr<HttpResponse>> sync_responses_;
};

}  // namespace network
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_TEST_FAKE_HTTP_CLIENT_H_
