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

#include "fastpair/repository/fast_pair_metadata_fetcher_impl.h"

#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "fastpair/common/fast_pair_http_result.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

namespace {

const char kQueryParameterAlternateOutputKey[] = "alt";
const char kQueryParameterAlternateOutputProto[] = "proto";
const char kPlatformTypeHeaderName[] = "X-Sharing-Platform-Type";

absl::string_view GetPlatformTypeString(api::DeviceInfo::OsType os_type) {
  switch (os_type) {
    case api::DeviceInfo::OsType::kAndroid:
      return "OSType.ANDROID";
    case api::DeviceInfo::OsType::kChromeOs:
      return "OSType.CHROME_OS";
    case api::DeviceInfo::OsType::kIos:
      return "OSType.IOS";
    case api::DeviceInfo::OsType::kWindows:
      return "OSType.WINDOWS";
    default:
      return "OSType.UNKNOWN";
  }
}

}  // namespace

void FastPairMetadataFetcherImpl::StartGetUnauthRequest(
    const network::Url& request_url,
    const QueryParameters& request_as_query_parameters,
    network::HttpClient* http_client, ResultCallback result_callback,
    ErrorCallback error_callback) {
  request_url_ = request_url;
  request_as_query_parameters_ = request_as_query_parameters;
  http_client_ = http_client;
  result_callback_ = result_callback;
  error_callback_ = error_callback;
  Execute();
}

void FastPairMetadataFetcherImpl::ProcessApiCallSuccess(
    const network::HttpResponse* response) {
  network::HttpResponseBody response_body = response->GetBody();
  result_callback_(response_body.GetRawData());
}

void FastPairMetadataFetcherImpl::ProcessApiCallFailure(absl::Status status) {
  std::optional<FastPairHttpError> error;
  std::string error_message;
  error = FastPairHttpErrorForHttpResponseCode(status);

  NEARBY_LOGS(ERROR) << "Fetcher failed: "
                     << FastPairHttpStatus(status).ToString();
  if (status.code() == absl::StatusCode::kFailedPrecondition) {
    error = FastPairHttpError::kHttpErrorOffline;
  }

  error_callback_(*error);
}

void FastPairMetadataFetcherImpl::Execute() {
  network::HttpRequest request{request_url_};
  request.AddQueryParameter(kQueryParameterAlternateOutputKey,
                            kQueryParameterAlternateOutputProto);
  if (request_as_query_parameters_) {
    for (const auto& key_value_pair : *request_as_query_parameters_) {
      request.AddQueryParameter(key_value_pair.first, key_value_pair.second);
    }
  }

  request.AddHeader(kPlatformTypeHeaderName, GetPlatformTypeString(os_type_));
  request.AddHeader("Content-Type", std::string());

  // Handle request body
  request.SetBody(std::string());

  http_client_->StartRequest(
      request, [&](const absl::StatusOr<network::HttpResponse>& response) {
        response_ = response;
        if (response_.ok()) {
          body_string_ = std::string(response_->GetBody().GetRawData());
          ProcessApiCallSuccess(&response_.value());
        } else {
          ProcessApiCallFailure(response.status());
        }
      });
}

}  // namespace fastpair
}  // namespace nearby
