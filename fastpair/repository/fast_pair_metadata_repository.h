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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_METADATA_REPOSITORY_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_METADATA_REPOSITORY_H_

#include <functional>
#include <memory>

#include "absl/strings/string_view.h"
#include "fastpair/common/fast_pair_http_result.h"

namespace nearby {
namespace fastpair {

namespace proto {

class GetObservedDeviceRequest;
class GetObservedDeviceResponse;

}  // namespace proto

class FastPairMetadataRepository {
 public:
  using ErrorCallback = std::function<void(FastPairHttpError)>;
  using ObservedDeviceCallback =
      std::function<void(const proto::GetObservedDeviceResponse&)>;
  FastPairMetadataRepository() = default;
  virtual ~FastPairMetadataRepository() = default;

  virtual void GetObservedDevice(const proto::GetObservedDeviceRequest& request,
                                 ObservedDeviceCallback callback,
                                 ErrorCallback error_callback) = 0;
};

// Interface for creating FastPairMetadataRepository instances to increase the
// flexibility
class FastPairMetadataRepositoryFactory {
 public:
  FastPairMetadataRepositoryFactory() = default;
  virtual ~FastPairMetadataRepositoryFactory() = default;

  virtual std::unique_ptr<FastPairMetadataRepository> CreateInstance() = 0;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_REPOSITORY_FAST_PAIR_METADATA_REPOSITORY_H_
