// Copyright 2023 Google LLC
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

#include "fastpair/server_access/fast_pair_repository_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "fastpair/repository/fast_pair_metadata_repository.h"
#include "fastpair/repository/fast_pair_metadata_repository_impl.h"
#include "fastpair/server_access/fast_pair_metadata_downloader_impl.h"
#include "internal/network/http_client_factory.h"
#include "internal/network/http_client_factory_impl.h"
#include "internal/platform/logging.h"
#include "absl/strings/string_view.h"

namespace nearby {
namespace fastpair {

FastPairRepositoryImpl::FastPairRepositoryImpl()
    : http_factory_(std::make_unique<nearby::network::HttpClientFactoryImpl>()),
      repository_factory_(
          std::make_unique<FastPairMetadataRepositoryFactoryImpl>(
              http_factory_.get())) {}

FastPairRepositoryImpl::FastPairRepositoryImpl(
    std::unique_ptr<FastPairMetadataRepositoryFactory> repository)
    : repository_factory_(std::move(repository)) {}

void FastPairRepositoryImpl::GetDeviceMetadata(
    absl::string_view hex_model_id,
    DeviceMetadataCallback callback) {
  downloader_ = FastPairMetadataDownloaderImpl::Factory::Create(
      hex_model_id, repository_factory_.get(), std::move(callback), []() {
        NEARBY_LOGS(INFO) << __func__
                          << ": Fast Pair Metadata download failed.";
      });
  downloader_->Run();
}
}  // namespace fastpair
}  // namespace nearby
