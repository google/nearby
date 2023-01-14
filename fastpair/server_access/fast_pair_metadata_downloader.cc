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

#include "fastpair/server_access/fast_pair_metadata_downloader.h"

#include <string>
#include <utility>
#include <optional>

#include "fastpair/proto/fastpair_rpcs.pb.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {
FastPairMetadataDownloader::FastPairMetadataDownloader(
    std::optional<std::string> model_id, SuccessCallback success_callback,
    FailureCallback failure_callback)
    : model_id_(model_id),
      success_callback_(std::move(success_callback)),
      failure_callback_(std::move(failure_callback)) {}

FastPairMetadataDownloader::~FastPairMetadataDownloader() = default;

void FastPairMetadataDownloader::Run() {
  DCHECK(!was_run_);
  was_run_ = true;
  OnRun();
}

void FastPairMetadataDownloader::Succeed(proto::Device device) {
  DCHECK(was_run_);
  DCHECK(success_callback_);

  std::move(success_callback_)(std::move(device));
}

void FastPairMetadataDownloader::Fail() {
  DCHECK(was_run_);
  DCHECK(failure_callback_);

  std::move(failure_callback_)();
}

}  // namespace fastpair
}  // namespace nearby
