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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_METADATA_FETCHER_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_METADATA_FETCHER_IMPL_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "fastpair/repository/fast_pair_metadata_fetcher.h"
#include "internal/network/http_client.h"
#include "internal/network/url.h"
#include "internal/platform/implementation/device_info.h"

namespace nearby {
namespace fastpair {

class FastPairMetadataFetcherImpl : public FastPairMetadataFetcher {
 public:
  explicit FastPairMetadataFetcherImpl(api::DeviceInfo::OsType os_type)
      : os_type_(os_type) {}
  ~FastPairMetadataFetcherImpl() override = default;
  FastPairMetadataFetcherImpl(const FastPairMetadataFetcherImpl&) = delete;
  FastPairMetadataFetcherImpl& operator=(const FastPairMetadataFetcherImpl&) =
      delete;
  // FastPairApiCallFlow
  void StartGetUnauthRequest(const network::Url& request_url,
                             const QueryParameters& request_as_query_parameters,
                             network::HttpClient* http_client,
                             ResultCallback result_callback,
                             ErrorCallback error_callback) override;

 protected:
  network::HttpRequestMethod GetRequestMethod() const;
  void ProcessApiCallSuccess(const network::HttpResponse* response);
  void ProcessApiCallFailure(absl::Status status);
  void Execute();

 private:
  // The URL of the endpoint serving the request.
  network::Url request_url_;

  // The request message proto represented as key-value pairs that will be sent
  // as query parameters in the API GET request. Note: A key can have multiple
  // values.
  std::optional<QueryParameters> request_as_query_parameters_;

  // Callback invoked with the serialized response message proto when the flow
  // completes successfully.
  ResultCallback result_callback_;

  // Callback invoked with an error message when the flow fails.
  ErrorCallback error_callback_;

  // Http client to execute request
  network::HttpClient* http_client_ = nullptr;

  // Store a copy of HTTP response
  absl::StatusOr<network::HttpResponse> response_;
  std::string body_string_;

  // Used to indicate the OS type.
  const api::DeviceInfo::OsType os_type_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_METADATA_FETCHER_IMPL_H_
