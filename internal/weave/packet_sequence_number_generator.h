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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_WEAVE_PACKET_SEQUENCE_NUMBER_GENERATOR_H_
#define THIRD_PARTY_NEARBY_INTERNAL_WEAVE_PACKET_SEQUENCE_NUMBER_GENERATOR_H_

#include "absl/base/thread_annotations.h"
#include "internal/platform/mutex.h"

namespace nearby {
namespace weave {

// Keeps a counter of the next Weave packet to set the new Weave packet sequence
// number in a connection. This counter automatically rolls over once it hits 7,
// since 3 bits are allocated in the packet to the counter, making its max value
// 0b111, or 7.
class PacketSequenceNumberGenerator {
 public:
  // Returns the next counter in the sequence, incrementing the counter
  // internally.
  int Next();

  // Resets the counter to 0.
  void Reset();

 private:
  Mutex mutex_;
  int packet_counter_ ABSL_GUARDED_BY(mutex_) = 0;
};

}  // namespace weave
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_WEAVE_PACKET_SEQUENCE_NUMBER_GENERATOR_H_
