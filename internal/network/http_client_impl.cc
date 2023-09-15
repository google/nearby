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

#include "internal/network/http_client_impl.h"

#include <memory>
#include <ostream>
#include <sstream>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "internal/network/debug.h"
#include "internal/network/http_request.h"
#include "internal/network/http_response.h"
#include "internal/network/http_status_code.h"
#include "internal/platform/implementation/http_loader.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace network {

void NearbyHttpClient::StartRequest(
    const HttpRequest& request,
    absl::AnyInvocable<void(const absl::StatusOr<HttpResponse>&)> callback) {
  MutexLock lock(&mutex_);
  executor_.Execute(
      [request = std::move(request), callback = std::move(callback)]() mutable {
        NEARBY_LOGS(INFO) << __func__ << ": Start async request to url="
                          << request.GetUrl().GetUrlPath();
        absl::StatusOr<HttpResponse> response = InternalGetResponse(request);
        if (response.ok()) {
          NEARBY_LOGS(INFO)
              << __func__
              << ": Got response from url=" << request.GetUrl().GetUrlPath();
        } else {
          NEARBY_LOGS(ERROR) << __func__ << ": Failed to get response from url="
                             << request.GetUrl().GetUrlPath() << ", status"
                             << response.status();
        }

        if (callback) {
          callback(response);
        }
        NEARBY_LOGS(INFO) << __func__ << ": Completed request to url="
                          << request.GetUrl().GetUrlPath();
      });
}

void NearbyHttpClient::StartCancellableRequest(
    std::unique_ptr<CancellableRequest> cancellable_request,
    absl::AnyInvocable<void(const absl::StatusOr<HttpResponse>&)> callback) {
  MutexLock lock(&mutex_);
  if (cancellable_request == nullptr) {
    NEARBY_LOGS(ERROR) << __func__ << ": invalid cancellable request.";
    callback(absl::InvalidArgumentError("invalid cancellable request"));
    return;
  }
  executor_
      .Execute(
          [cancellable_request = std::move(cancellable_request),
           callback = std::move(callback)]() mutable {
            NEARBY_LOGS(INFO)
                << __func__ << ": Start async request to url="
                << cancellable_request->http_request().GetUrl().GetUrlPath();
            if (cancellable_request->is_cancelled()) {
              NEARBY_LOGS(WARNING)
                  << __func__ << ": Async request to url="
                  << cancellable_request->http_request().GetUrl().GetUrlPath()
                  << " is cancelled.";
              return;
            }
            absl::StatusOr<HttpResponse> response =
                InternalGetResponse(cancellable_request->http_request());
            if (response.ok()) {
              NEARBY_LOGS(INFO)
                  << __func__ << ": Got response from url="
                  << cancellable_request->http_request().GetUrl().GetUrlPath();
            } else {
              NEARBY_LOGS(ERROR)
                  << __func__ << ": Failed to get response from url="
                  << cancellable_request->http_request().GetUrl().GetUrlPath()
                  << ", status" << response.status();
            }

            if (cancellable_request->is_cancelled()) {
              NEARBY_LOGS(WARNING)
                  << __func__ << ": Async request to url="
                  << cancellable_request->http_request().GetUrl().GetUrlPath()
                  << " is cancelled.";
              return;
            }

            if (callback) {
              callback(response);
            }
            NEARBY_LOGS(INFO)
                << __func__ << ": Completed request to url="
                << cancellable_request->http_request().GetUrl().GetUrlPath();
          });
}

absl::StatusOr<HttpResponse> NearbyHttpClient::GetResponse(
    const HttpRequest& request) {
  NEARBY_LOGS(INFO) << __func__ << ": Start request to url="
                    << request.GetUrl().GetUrlPath();

  absl::StatusOr<HttpResponse> response = InternalGetResponse(request);
  if (response.ok()) {
    NEARBY_LOGS(INFO) << __func__ << ": Got response from url="
                      << request.GetUrl().GetUrlPath();
  } else {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to get response from url="
                       << request.GetUrl().GetUrlPath() << ", status"
                       << response.status();
  }

  return response;
}

absl::StatusOr<HttpResponse> NearbyHttpClient::InternalGetResponse(
    const HttpRequest& request) {
  api::WebRequest web_request;
  web_request.url = request.GetUrl().GetUrlPath();
  web_request.method = absl::StrCat(request.GetMethodString());
  for (const auto& header : request.GetAllHeaders()) {
    for (const auto& value : header.second) {
      web_request.headers.emplace(header.first, value);
    }
  }
  web_request.body = absl::StrCat(request.GetBody().GetRawData());

  if (debug::kRequestEnabled) {
    std::stringstream request_stream;
    request_stream << "HTTP REQUEST====>" << std::endl;
    request_stream << web_request.method << " " << web_request.url << std::endl;
    for (const auto& header : web_request.headers) {
      request_stream << header.first << ": " << header.second << std::endl;
    }
    request_stream << std::endl;
    request_stream << "body size: " << request.GetBody().GetRawData().size()
                   << std::endl;
    NEARBY_LOGS(VERBOSE) << request_stream.str();
  }

  absl::StatusOr<api::WebResponse> web_response =
      api::ImplementationPlatform::SendRequest(web_request);

  if (!web_response.ok()) {
    return web_response.status();
  }

  if (debug::kResponseEnabled) {
    std::stringstream response_stream;
    response_stream << "HTTP RESPONSE====>" << std::endl;
    response_stream << "url: " << web_request.url << std::endl;
    response_stream << web_response->status_code << " "
                    << web_response->status_text << std::endl;
    for (const auto& header : web_response->headers) {
      response_stream << header.first << ": " << header.second << std::endl;
    }
    response_stream << std::endl;
    response_stream << "body size: " << web_response->body.size() << std::endl;
    NEARBY_LOGS(VERBOSE) << response_stream.str();
  }

  HttpResponse response;

  response.SetStatusCode(
      static_cast<HttpStatusCode>(web_response->status_code));
  response.SetReasonPhrase(web_response->status_text);
  for (const auto& header : web_response->headers) {
    response.AddHeader(header.first, header.second);
  }
  response.SetBody(web_response->body);

  return response;
}

}  // namespace network
}  // namespace nearby
