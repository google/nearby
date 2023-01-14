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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAKE_FAST_PAIR_METADATA_REPOSITORY_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAKE_FAST_PAIR_METADATA_REPOSITORY_H_

#include <memory>
#include <utility>

#include "fastpair/repository/fast_pair_metadata_repository.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"


namespace nearby {
namespace fastpair {
class FakeFastPairMetadataRepository : public FastPairMetadataRepository {
 public:
  struct GetObservedDeviceRequest {
    GetObservedDeviceRequest(const proto::GetObservedDeviceRequest& request,
                             ObservedDeviceCallback callback,
                             ErrorCallback error_callback)
        : request(request),
          callback(std::move(callback)),
          error_callback(std::move(error_callback)) {}

    GetObservedDeviceRequest(GetObservedDeviceRequest&& request) = default;
    ~GetObservedDeviceRequest() = default;

    proto::GetObservedDeviceRequest request;
    ObservedDeviceCallback callback;
    ErrorCallback error_callback;
  };

  FakeFastPairMetadataRepository() = default;
  ~FakeFastPairMetadataRepository() override = default;

  std::unique_ptr<GetObservedDeviceRequest>& get_observed_device_request() {
    return get_observed_device_request_;
  }

 private:
  void GetObservedDevice(const proto::GetObservedDeviceRequest& request,
                         ObservedDeviceCallback response_callback,
                         ErrorCallback error_callback) override {
    get_observed_device_request_ = std::make_unique<GetObservedDeviceRequest>(
        request, std::move(response_callback), std::move(error_callback));
  }

  std::unique_ptr<GetObservedDeviceRequest> get_observed_device_request_;
};

class FakeFastPairMetadataRepositoryFactory
    : public FastPairMetadataRepositoryFactory {
 public:
  FakeFastPairMetadataRepositoryFactory() = default;
  ~FakeFastPairMetadataRepositoryFactory() override = default;

  FakeFastPairMetadataRepository* fake_repository() { return fake_repository_; }

 private:
  std::unique_ptr<FastPairMetadataRepository> CreateInstance() override {
    auto fake_repository = std::make_unique<FakeFastPairMetadataRepository>();
    fake_repository_ = fake_repository.get();
    return fake_repository;
  }
  FakeFastPairMetadataRepository* fake_repository_;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAKE_FAST_PAIR_METADATA_REPOSITORY_H_
