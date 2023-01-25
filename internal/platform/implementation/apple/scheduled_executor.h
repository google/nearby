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

#ifndef PLATFORM_IMPL_APPLE_SCHEDULED_EXECUTOR_H_
#define PLATFORM_IMPL_APPLE_SCHEDULED_EXECUTOR_H_

#import <Foundation/Foundation.h>

#include <memory>

#include "internal/platform/implementation/scheduled_executor.h"
#include "internal/platform/runnable.h"

/**
 * The impl class is an Obj-C class so that
 *  (a) the dispatch block can strongly retain it, and
 *  (b) for ease of declaring an atomic property.
 */
@interface GNCOperationQueueImpl : NSObject
@property(nonatomic) NSOperationQueue* queue;
@property(atomic) BOOL shuttingDown;
@end

namespace nearby {
namespace apple {

// Concrete ScheduledExecutor implementation.
class ScheduledExecutor : public api::ScheduledExecutor {
 public:
  // The max_concurrency = 1 for default constructor.
  ScheduledExecutor();
  explicit ScheduledExecutor(int max_concurrency);
  ~ScheduledExecutor() override;

  ScheduledExecutor(const ScheduledExecutor&) = delete;
  ScheduledExecutor& operator=(const ScheduledExecutor&) = delete;

  // api::ScheduledExecutor:
  void Shutdown() override;
  std::shared_ptr<api::Cancelable> Schedule(Runnable&& runnable, absl::Duration duration) override;
  void Execute(Runnable&& runnable) override;

  bool DoSubmit(Runnable&& runnable);

 private:
  void Shutdown(std::int64_t timeout_millis);

  GNCOperationQueueImpl* impl_;
};

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_SCHEDULED_EXECUTOR_H_
