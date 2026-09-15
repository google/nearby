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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_METADATA_MANAGER_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_METADATA_MANAGER_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "fastpair/proto/fastpair_rpcs.pb.h"
#include "fastpair/proto/fastpair_rpcs.proto.h"
#include "fastpair/repository/fast_pair_metadata_repository.h"
#include "fastpair/server_access/fast_pair_metadata_downloader.h"
#include "fastpair/server_access/fast_pair_metadata_manager.h"
#include "internal/base/observer_list.h"

namespace nearby {
namespace fastpair {

class FastPairMetadataManagerImpl : public FastPairMetadataManager,
                                    public FastPairMetadataManager::Observer {
 public:
  class Factory {
   public:
    static std::unique_ptr<FastPairMetadataManager> Create(
        std::optional<std::string> model_id,
        FastPairMetadataRepositoryFactory* repository_factory);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<FastPairMetadataManager> CreateInstance(
        std::optional<std::string> model_id,
        FastPairMetadataRepositoryFactory* repository_factory) = 0;

   private:
    static Factory* test_factory_;
  };

  ~FastPairMetadataManagerImpl() override;

 private:
  FastPairMetadataManagerImpl(
      std::optional<std::string> model_id,
      FastPairMetadataRepositoryFactory* repository_factory);

  void DownloadMetadata() override;
  void OnStart() override;
  void OnStop() override;

  void OnMetadataDownloadRequested();
  void OnMetadataDownloadFinished(
      const std::optional<proto::GetObservedDeviceResponse>& response);
  void OnMetadataDownlowndSuccess(proto::Device device);
  void OnMetadataDownlowndFailure();

  void OnMetadataDownlownd(const std::optional<std::string>& model_id,
                           const proto::Device& device) override;

  void NotifyAllObserversMetadataDownloaded(
      const std::optional<std::string>& model_id, const proto::Device& device);

  std::optional<std::string> model_id_;
  std::unique_ptr<FastPairMetadataRepository> repository_;
  FastPairMetadataRepositoryFactory* repository_factory_ = nullptr;
  std::unique_ptr<FastPairMetadataDownloader> downloader_;
};

}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_SERVER_ACCESS_FAST_PAIR_METADATA_MANAGER_IMPL_H_
