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

#include "fastpair/repository/fast_pair_metadata_repository_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/fast_pair_http_result.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/repository/fast_pair_metadata_fetcher.h"
#include "fastpair/repository/fast_pair_metadata_fetcher_impl.h"
#include "fastpair/repository/fast_pair_metadata_repository.h"
#include "internal/network/http_client.h"
#include "internal/network/http_client_factory.h"
#include "internal/network/url.h"
#include "internal/platform/implementation/device_info.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

const char kDefaultNearbyDeviceV1HTTPHost[] =
    "https://nearbydevices-pa.googleapis.com";

const char kNearbyShareV1Path[] = "v1";
const char kListDevicePath[] = "device";
const char kClientId[] = "AIzaSyBv7ZrOlX5oIJLVQrZh-WkZFKm5L6FlStQ";
const char kMode[] = "mode";
const char* GetObservedDeviceMode[3] = {"MODE_UNKNOWN", "MODE_RELEASE",
                                        "MODE_DEBUG"};
FastPairMetadataRepositoryImpl::FastPairMetadataRepositoryImpl(
    std::unique_ptr<FastPairMetadataFetcher> fetcher,
    std::unique_ptr<network::HttpClient> http_client)
    : fetcher_(std::move(fetcher)), http_client_(std::move(http_client)) {}

FastPairMetadataRepositoryImpl::~FastPairMetadataRepositoryImpl() = default;

// Create full fastpair v1 URL
nearby::network::Url CreateV1DeviceRequestUrlPath(
    const proto::GetObservedDeviceRequest& request) {
  std::string path = absl::StrFormat(
      "%s/%s/%s/%s", kDefaultNearbyDeviceV1HTTPHost, kNearbyShareV1Path,
      kListDevicePath, std::to_string(request.device_id()));
  return nearby::network::Url::Create(path).value();
}

FastPairMetadataFetcher::QueryParameters
GetObservedDeviceRequestToQueryParameters(
    const proto::GetObservedDeviceRequest& request) {
  FastPairMetadataFetcher::QueryParameters params;
  params.push_back({"key", kClientId});
  params.push_back({kMode, GetObservedDeviceMode[request.mode()]});

  return params;
}

void FastPairMetadataRepositoryImpl::GetObservedDevice(
    const proto::GetObservedDeviceRequest& request,
    ObservedDeviceCallback callback, ErrorCallback error_callback) {
  ObservedDeviceCallback new_callback =
      [=](const proto::GetObservedDeviceResponse& response) {
        callback(response);
      };
  ErrorCallback new_error_callback = [=](FastPairHttpError error) {
    error_callback(error);
  };

  MakeUnauthFetcher(CreateV1DeviceRequestUrlPath(request),
                    GetObservedDeviceRequestToQueryParameters(request),
                    new_callback, new_error_callback);
}

void FastPairMetadataRepositoryImpl::MakeUnauthFetcher(
    const nearby::network::Url& request_url,
    const std::optional<FastPairMetadataFetcher::QueryParameters>&
        request_as_query_parameters,
    ObservedDeviceCallback response_callback, ErrorCallback error_callback) {
  has_call_started_ = true;
  request_url_ = request_url;
  error_callback_ = std::move(error_callback);
  fetcher_->StartGetUnauthRequest(
      request_url_, *request_as_query_parameters, http_client_.get(),
      [&, response_callback =
              std::move(response_callback)](absl::string_view result_response) {
        OnFetcherSuccess(response_callback, result_response);
      },
      [&](FastPairHttpError error) { OnFetcherFailed(error); });
}

void FastPairMetadataRepositoryImpl::OnFetcherSuccess(
    ObservedDeviceCallback callback, absl::string_view result_response) {
  proto::GetObservedDeviceResponse response;
  if (!response.ParseFromString(result_response)) {
    OnFetcherFailed(FastPairHttpError::kHttpErrorOtherFailure);
    return;
  }
  callback(response);
}

void FastPairMetadataRepositoryImpl::OnFetcherFailed(FastPairHttpError error) {
  NEARBY_LOGS(ERROR)
      << "Fast pair server accessing RPC call failed with error. " << error;
  error_callback_(error);
}

FastPairMetadataRepositoryFactoryImpl::FastPairMetadataRepositoryFactoryImpl(
    network::HttpClientFactory* http_client_factory)
    : http_client_factory_(http_client_factory) {}

FastPairMetadataRepositoryFactoryImpl::
    ~FastPairMetadataRepositoryFactoryImpl() = default;

std::unique_ptr<FastPairMetadataRepository>
FastPairMetadataRepositoryFactoryImpl::CreateInstance() {
  NEARBY_LOGS(VERBOSE) << "Device Type is hardcode to kWindows.";
  return std::make_unique<FastPairMetadataRepositoryImpl>(
      std::make_unique<FastPairMetadataFetcherImpl>(
          api::DeviceInfo::OsType::kWindows),
      http_client_factory_->CreateInstance());
}

}  // namespace fastpair
}  // namespace nearby
