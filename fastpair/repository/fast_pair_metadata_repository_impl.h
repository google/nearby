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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_METADATA_REPOSITORY_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_METADATA_REPOSITORY_IMPL_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "fastpair/repository/fast_pair_metadata_fetcher.h"
#include "fastpair/repository/fast_pair_metadata_repository.h"
#include "internal/network/http_client.h"
#include "internal/network/http_client_factory.h"

namespace nearby {
namespace fastpair {

class FastPairMetadataRepositoryImpl : public FastPairMetadataRepository {
 public:
  FastPairMetadataRepositoryImpl(
      std::unique_ptr<FastPairMetadataFetcher> fetcher,
      std::unique_ptr<network::HttpClient> http_client);
  ~FastPairMetadataRepositoryImpl() override;

  FastPairMetadataRepositoryImpl(FastPairMetadataRepositoryImpl&) = delete;
  FastPairMetadataRepositoryImpl& operator=(FastPairMetadataRepositoryImpl&) =
      delete;
  void GetObservedDevice(const proto::GetObservedDeviceRequest& request,
                         ObservedDeviceCallback response_callback,
                         ErrorCallback error_callback) override;

 private:
  void MakeUnauthFetcher(
      const nearby::network::Url& request_url,
      const std::optional<FastPairMetadataFetcher::QueryParameters>&
          request_as_query_parameters,
      ObservedDeviceCallback response_callback, ErrorCallback error_callback);

  // Called when the fetcher success fails at any step.
  void OnFetcherSuccess(ObservedDeviceCallback callback,
                        absl::string_view result_response);

  // Called when the fetcher fails at any step.
  void OnFetcherFailed(FastPairHttpError error);

  bool has_call_started_;
  std::unique_ptr<FastPairMetadataFetcher> fetcher_;
  std::unique_ptr<nearby::network::HttpClient> http_client_;
  nearby::network::Url request_url_;
  ErrorCallback error_callback_;
};

class FastPairMetadataRepositoryFactoryImpl
    : public FastPairMetadataRepositoryFactory {
 public:
  explicit FastPairMetadataRepositoryFactoryImpl(
      network::HttpClientFactory* http_client_factory);
  ~FastPairMetadataRepositoryFactoryImpl() override;

  FastPairMetadataRepositoryFactoryImpl(
      FastPairMetadataRepositoryFactoryImpl&) = delete;
  FastPairMetadataRepositoryFactoryImpl& operator=(
      FastPairMetadataRepositoryFactoryImpl&) = delete;

  std::unique_ptr<FastPairMetadataRepository> CreateInstance() override;

 private:
  network::HttpClientFactory* http_client_factory_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_METADATA_REPOSITORY_IMPL_H_
