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

#ifndef PLATFORM_API2_ATOMIC_REFERENCE_H_
#define PLATFORM_API2_ATOMIC_REFERENCE_H_

namespace location {
namespace nearby {

// An object reference that may be updated atomically.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/atomic/AtomicReference.html
template <typename T>
class AtomicReference {
 public:
  virtual ~AtomicReference() {}

  virtual T Get() = 0;
  virtual void Set(const T& value) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API2_ATOMIC_REFERENCE_H_
