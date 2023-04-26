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

#include "fastpair/handshake/fast_pair_handshake_lookup.h"

#include <memory>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "fastpair/handshake/fast_pair_handshake_impl.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace fastpair {

FastPairHandshakeLookup* FastPairHandshakeLookup::instance_ = nullptr;
absl::Mutex FastPairHandshakeLookup::mutex_(absl::kConstInit);

// static
FastPairHandshakeLookup* FastPairHandshakeLookup::GetInstance() {
  absl::MutexLock lock(&mutex_);
  if (!instance_) {
    instance_ = new FastPairHandshakeLookup();
  }
  return instance_;
}

FastPairHandshake* FastPairHandshakeLookup::Get(FastPairDevice* device) {
  absl::MutexLock lock(&mutex_);
  auto it = fast_pair_handshakes_.find(device);
  return it != fast_pair_handshakes_.end() ? it->second.get() : nullptr;
}

FastPairHandshake* FastPairHandshakeLookup::Get(absl::string_view address) {
  absl::MutexLock lock(&mutex_);
  for (const auto& pair : fast_pair_handshakes_) {
    if (pair.first->public_address() == address ||
        pair.first->GetBleAddress() == address) {
      return pair.second.get();
    }
  }
  return nullptr;
}

bool FastPairHandshakeLookup::Erase(FastPairDevice* device) {
  absl::MutexLock lock(&mutex_);
  return fast_pair_handshakes_.erase(device) == 1;
}

bool FastPairHandshakeLookup::Erase(absl::string_view address) {
  absl::MutexLock lock(&mutex_);
  for (const auto& pair : fast_pair_handshakes_) {
    if (pair.first->public_address() == address ||
        pair.first->GetBleAddress() == address) {
      fast_pair_handshakes_.erase(pair.first);
      return true;
    }
  }
  return false;
}

void FastPairHandshakeLookup::Clear() {
  absl::MutexLock lock(&mutex_);
  fast_pair_handshakes_.clear();
}

FastPairHandshake* FastPairHandshakeLookup::Create(
    FastPairDevice& device, OnCompleteCallback on_complete) {
  absl::MutexLock lock(&mutex_);
  auto it = fast_pair_handshakes_.emplace(
      &device,
      std::make_unique<FastPairHandshakeImpl>(device, std::move(on_complete)));
  DCHECK(it.second);
  return it.first->second.get();
}

}  // namespace fastpair
}  // namespace nearby
