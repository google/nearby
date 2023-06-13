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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_LOOKUP_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_LOOKUP_H_

#include <functional>
#include <memory>
#include <optional>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "fastpair/common/fast_pair_device.h"
#include "fastpair/common/pair_failure.h"
#include "fastpair/handshake/fast_pair_handshake.h"
#include "fastpair/internal/mediums/mediums.h"

namespace nearby {
namespace fastpair {

// This Singletonclass creates, deletes and exposes FastPairHandshake instances.
class FastPairHandshakeLookup {
 public:
  using OnCompleteCallback = absl::AnyInvocable<void(
      FastPairDevice& device, std::optional<PairFailure> failure)>;

  using CreateFunction = absl::AnyInvocable<std::unique_ptr<FastPairHandshake>(
      FastPairDevice& device, Mediums& mediums, OnCompleteCallback callback)>;

  // This is the static method that controls the access to the singleton
  // instance. On the first run, it creates a singleton object and places it
  // into the static field. On subsequent runs, it returns the existing object
  // stored in the static field.
  static FastPairHandshakeLookup* GetInstance();

  static void SetCreateFunctionForTesting(CreateFunction create_function);

  // Singletons should not be cloneable.
  FastPairHandshakeLookup(const FastPairHandshakeLookup&) = delete;
  // Singletons should not be assignable.
  FastPairHandshakeLookup& operator=(const FastPairHandshakeLookup&) = delete;

  // Get an existing instance for |FastPairdevice|.
  FastPairHandshake* Get(FastPairDevice* device);

  // Get an existing instance for |address|.
  FastPairHandshake* Get(absl::string_view address);

  // Erases the FastPairHandshake instance for |FastPairdevice| if it exists.
  bool Erase(FastPairDevice* device);

  // Erases the FastPairHandshake instance for |FastPairdevice| if it exists.
  bool Erase(absl::string_view address);

  // Deletes all existing FastPairHandshake instances.
  void Clear();

  // Creates and returns a new instance for |FastPairdevice| if no instance
  // already exists.
  // Returns the existing instance if there is one.
  FastPairHandshake* Create(FastPairDevice& device, Mediums& mediums,
                            OnCompleteCallback on_complete,
                            SingleThreadExecutor* executor);

 protected:
  // Constructor/destructor of singleton object should not be public
  // for which the destructor will never be called.
  // and constructor will be invoked once from GetInstance() static method.
  FastPairHandshakeLookup() = default;
  ~FastPairHandshakeLookup() = default;

 private:
  static absl::Mutex mutex_;
  static FastPairHandshakeLookup* instance_ ABSL_GUARDED_BY(mutex_);

  absl::flat_hash_map<FastPairDevice*, std::unique_ptr<FastPairHandshake>>
      fast_pair_handshakes_ ABSL_GUARDED_BY(mutex_);
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_HANDSHAKE_FAST_PAIR_HANDSHAKE_LOOKUP_H_
