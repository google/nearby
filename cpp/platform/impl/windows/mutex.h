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

#ifndef PLATFORM_IMPL_WINDOWS_MUTEX_H_
#define PLATFORM_IMPL_WINDOWS_MUTEX_H_

#include "platform/api/mutex.h"

namespace location {
namespace nearby {
namespace windows {

// A lock is a tool for controlling access to a shared resource by multiple
// threads.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/locks/Lock.html
class Mutex : public api::Mutex {
 public:
  // TODO(b/184975123): replace with real implementation.
  ~Mutex() override = default;

  // TODO(b/184975123): replace with real implementation.
  void Lock() override {}
  // TODO(b/184975123): replace with real implementation.
  void Unlock() override {}
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_IMPL_WINDOWS_MUTEX_H_
