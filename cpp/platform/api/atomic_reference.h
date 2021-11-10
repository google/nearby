// Copyright 2020 Google LLC
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

#ifndef PLATFORM_API_ATOMIC_REFERENCE_H_
#define PLATFORM_API_ATOMIC_REFERENCE_H_

#include <cstdint>

namespace nearby {
namespace api {

// Type that allows 32-bit atomic reads and writes.
class AtomicUint32 {
 public:
  virtual ~AtomicUint32() = default;

  // Atomically reads and returns stored value.
  virtual std::uint32_t Get() const = 0;

  // Atomically stores value.
  virtual void Set(std::uint32_t value) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_ATOMIC_REFERENCE_H_
