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

#include "fastpair/server_access/fast_pair_metadata_manager_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "fastpair/repository/fast_pair_metadata_repository.h"
#include "fastpair/server_access/fast_pair_metadata_downloader_impl.h"
#include "fastpair/server_access/fast_pair_metadata_manager.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

FastPairMetadataManagerImpl::Factory*
    FastPairMetadataManagerImpl::Factory::test_factory_ = nullptr;

std::unique_ptr<FastPairMetadataManager>
FastPairMetadataManagerImpl::Factory::Create(
    std::optional<std::string> model_id,
    FastPairMetadataRepositoryFactory* repository_factory) {
  if (test_factory_) {
    return test_factory_->CreateInstance(model_id, repository_factory);
  }

  return absl::WrapUnique(
      new FastPairMetadataManagerImpl(model_id, repository_factory));
}

void FastPairMetadataManagerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

FastPairMetadataManagerImpl::Factory::~Factory() = default;

FastPairMetadataManagerImpl::~FastPairMetadataManagerImpl() = default;

FastPairMetadataManagerImpl::FastPairMetadataManagerImpl(
    std::optional<std::string> model_id,
    FastPairMetadataRepositoryFactory* repository_factory)
    : model_id_(model_id), repository_factory_(repository_factory) {
      OnMetadataDownloadRequested();
    }

void FastPairMetadataManagerImpl::DownloadMetadata() { Start(); }

void FastPairMetadataManagerImpl::OnStart() {/*scheduler start*/ }

void FastPairMetadataManagerImpl::OnStop() { /*scheduler stop*/ }

void FastPairMetadataManagerImpl::OnMetadataDownlownd(
    const std::optional<std::string>& model_id, const proto::Device& device) {}

void FastPairMetadataManagerImpl::OnMetadataDownloadRequested() {
  NEARBY_LOGS(VERBOSE) << __func__
                       << ": Fast Pair metadata download requested.";
  if (downloader_) {
    NEARBY_LOGS(WARNING) << __func__
                         << ": Ignored to download metadata again due to "
                            "downloading is in progress.";
    return;
  }

  downloader_ = FastPairMetadataDownloaderImpl::Factory::Create(
      model_id_, repository_factory_,
      [&](proto::Device device) {
        OnMetadataDownlowndSuccess(device);
        downloader_.reset();
      },
      [&] {
        OnMetadataDownlowndFailure();
        downloader_.reset();
      });
  downloader_->Run();
}

void FastPairMetadataManagerImpl::OnMetadataDownlowndSuccess(
    proto::Device device) {
  NEARBY_LOGS(INFO) << __func__ << ": Fast Pair download of " << device.name()
                    << " succeeded.";
  NotifyAllObserversMetadataDownloaded(model_id_, device);
}

void FastPairMetadataManagerImpl::OnMetadataDownlowndFailure() {
  NEARBY_LOGS(INFO) << __func__ << ": Fast Pair Metadata download failed.";
}

void FastPairMetadataManagerImpl::NotifyAllObserversMetadataDownloaded(
    const std::optional<std::string>& model_id, const proto::Device& device) {
  NotifyMetadataDownloaded(model_id, device);
}

}  // namespace fastpair
}  // namespace nearby
