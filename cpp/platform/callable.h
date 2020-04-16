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

#ifndef PLATFORM_CALLABLE_H_
#define PLATFORM_CALLABLE_H_

#include "platform/exception.h"

namespace location {
namespace nearby {

// The Callable interface should be implemented by any class whose instances are
// intended to be executed by a thread, and need to return a result. The class
// must define a method named call() with no arguments and a specific return
// type.
//
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Callable.html
template <typename T>
class Callable {
 public:
  virtual ~Callable() {}

  virtual ExceptionOr<T> call() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_CALLABLE_H_
