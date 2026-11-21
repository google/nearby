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

#ifndef PLATFORM_IMPL_APPLE_SINGLE_THREAD_EXECUTOR_H_
#define PLATFORM_IMPL_APPLE_SINGLE_THREAD_EXECUTOR_H_

#include <memory>

#include "absl/time/time.h"
#include "internal/platform/implementation/single_thread_executor.h"
#include "internal/platform/runnable.h"

namespace nearby {
namespace apple {

class ScheduledExecutor;

class SingleThreadExecutor final : public api::SingleThreadExecutor {
 public:
  SingleThreadExecutor();
  ~SingleThreadExecutor() override;

  SingleThreadExecutor(const SingleThreadExecutor&) = delete;
  SingleThreadExecutor& operator=(const SingleThreadExecutor&) = delete;

  // api::SingleThreadExecutor:
  bool Execute(Runnable&& runnable) override;
  bool DelayedExecute(absl::Duration delay, Runnable&& runnable) override;
  void Shutdown() override;
  bool IsRunningOnExecutor() const override;

 private:
  std::unique_ptr<ScheduledExecutor> scheduled_executor_;
};

}  // namespace apple
}  // namespace nearby

#endif  // PLATFORM_IMPL_APPLE_SINGLE_THREAD_EXECUTOR_H_
