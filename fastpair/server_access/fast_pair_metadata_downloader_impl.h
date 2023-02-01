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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_METADATA_DOWNLOADER_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_METADATA_DOWNLOADER_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"
#include "fastpair/common/fast_pair_http_result.h"
#include "fastpair/proto/fastpair_rpcs.pb.h"
#include "fastpair/repository/fast_pair_metadata_repository.h"
#include "fastpair/server_access/fast_pair_metadata_downloader.h"

namespace nearby {
namespace fastpair {

class FastPairMetadataRepository;

class FastPairMetadataDownloaderImpl : public FastPairMetadataDownloader {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairMetadataDownloader> Create(
        absl::string_view model_id,
        FastPairMetadataRepositoryFactory* repository_factory,
        SuccessCallback success_callback, FailureCallback failure_callback);

    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<FastPairMetadataDownloader> CreateInstance(
        absl::string_view model_id,
        FastPairMetadataRepositoryFactory* repository_factory,
        SuccessCallback success_callback, FailureCallback failure_callback) = 0;

   private:
    static Factory* test_factory_;
  };

  ~FastPairMetadataDownloaderImpl() override;

 private:
  FastPairMetadataDownloaderImpl(
      absl::string_view model_id,
      FastPairMetadataRepositoryFactory* repository_factory,
      SuccessCallback success_callback, FailureCallback failure_callback);

  void OnRun() override;
  void CallAccessServer(absl::string_view model_id);
  void OnAccessServerSuccess(const proto::GetObservedDeviceResponse& response);
  void OnAccessServerFailure(FastPairHttpError error);

  std::unique_ptr<FastPairMetadataRepository> repository_;
  FastPairMetadataRepositoryFactory* repository_factory_ = nullptr;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_METADATA_DOWNLOADER_IMPL_H_
