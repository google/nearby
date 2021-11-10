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

#ifndef PLATFORM_API_ATOMIC_BOOLEAN_H_
#define PLATFORM_API_ATOMIC_BOOLEAN_H_

namespace nearby {
namespace api {

// A boolean value that may be updated atomically.
class AtomicBoolean {
 public:
  virtual ~AtomicBoolean() = default;

  // Atomically read and return current value.
  virtual bool Get() const = 0;

  // Atomically exchange original value with a new one. Return previous value.
  virtual bool Set(bool value) = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // PLATFORM_API_ATOMIC_BOOLEAN_H_
