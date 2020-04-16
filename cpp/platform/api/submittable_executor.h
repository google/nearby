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

#ifndef PLATFORM_API_SUBMITTABLE_EXECUTOR_H_
#define PLATFORM_API_SUBMITTABLE_EXECUTOR_H_

#include "platform/api/executor.h"
#include "platform/api/future.h"
#include "platform/callable.h"
#include "platform/port/down_cast.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

// Each per-platform concrete implementation is expected to extend from
// SubmittableExecutor and provide an override of its submit() method.
//
// e.g.
// class IOSSubmittableExecutor
//                      : public SubmittableExecutor<IOSSubmittableExecutor> {
//  public:
//   template <typename T>
//   Ptr<Future<T> > submit(Ptr<Callable<T> > callable) {
//     ...
//   }
// }
template <typename ConcreteSubmittableExecutor>
class SubmittableExecutor : public Executor {
 public:
  ~SubmittableExecutor() override {}

  template <typename T>
  Ptr<Future<T>> submit(Ptr<Callable<T>> callable) {
    return DOWN_CAST<ConcreteSubmittableExecutor*>(this)->submit(callable);
  }
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_SUBMITTABLE_EXECUTOR_H_
