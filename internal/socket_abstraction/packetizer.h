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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_PACKETIZER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_PACKETIZER_H_

#include <vector>

#include "absl/status/statusor.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/mutex.h"
#include "internal/socket_abstraction/packet.h"

namespace nearby {
namespace socket_abstraction {
class Packetizer {
 public:
  absl::StatusOr<ByteArray> Join(Packet packet) ABSL_LOCKS_EXCLUDED(mutex_);
  void Reset() ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  Mutex mutex_;
  int pending_payloads_size_ ABSL_GUARDED_BY(mutex_) = 0;
  std::vector<ByteArray> pending_payloads_ ABSL_GUARDED_BY(mutex_) = {};
};
}  // namespace socket_abstraction
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_SOCKET_ABSTRACTION_PACKETIZER_H_
