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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_METADATA_FETCHER_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_METADATA_FETCHER_H_

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "fastpair/common/fast_pair_http_result.h"
#include "internal/network/http_client.h"
#include "internal/network/url.h"

namespace nearby {
namespace fastpair {

class FastPairMetadataFetcher {
 public:
  using ResultCallback = std::function<void(absl::string_view result_response)>;
  using ErrorCallback = std::function<void(FastPairHttpError error)>;
  using QueryParameters = std::vector<std::pair<std::string, std::string>>;

  FastPairMetadataFetcher() = default;
  virtual ~FastPairMetadataFetcher() = default;

  // Starts the API GET request call.
  //   |request_url|: The URL endpoint of the API request.
  //   |request_as_query_parameters|: The request proto represented as key-value
  //                                  pairs to be sent as query parameters.
  //                                  Note: A key can have multiple values.
  //   |http_client|: The HTTP client is used to access backend APIs.
  //   |result_callback|: Called when the flow completes successfully
  //                      with a serialized response proto.
  //   |error_callback|: Called when the flow completes with an error.
  virtual void StartGetUnauthRequest(
      const network::Url& request_url,
      const QueryParameters& request_as_query_parameters,
      network::HttpClient* http_client, ResultCallback result_callback,
      ErrorCallback error_callback) = 0;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_METADATA_FETCHER_H_
