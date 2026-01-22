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

#include "fastpair/server_access/fast_pair_metadata_manager.h"

#include <string>
#include <optional>

namespace nearby {
namespace fastpair {

FastPairMetadataManager::FastPairMetadataManager() = default;
FastPairMetadataManager::~FastPairMetadataManager() = default;

void FastPairMetadataManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FastPairMetadataManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FastPairMetadataManager::Start() {
  if (is_running_) return;
  is_running_ = true;
  OnStart();
}

void FastPairMetadataManager::Stop() {
  if (!is_running_) return;
  is_running_ = false;
  OnStop();
}

void FastPairMetadataManager::NotifyMetadataDownloaded(
    const std::optional<std::string>& model_id, const proto::Device& device) {
  for (Observer* observer : observers_) {
    observer->OnMetadataDownlownd(model_id, device);
  }
}

}  // namespace fastpair
}  // namespace nearby
