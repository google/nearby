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

#ifndef THIRD_PARTY_NEARBY_FASTPAIR_COMMON_CONTEXT_IMPL_H_
#define THIRD_PARTY_NEARBY_FASTPAIR_COMMON_CONTEXT_IMPL_H_

#include <memory>

#include "fastpair/common/context.h"

namespace nearby {
namespace fastpair {
class ContextImpl : public Context {
 public:
  ContextImpl();
  ~ContextImpl() override = default;

  Clock* GetClock() const override;
  std::unique_ptr<Timer> CreateTimer() override;
  DeviceInfo* GetDeviceInfo() const override;
  std::unique_ptr<TaskRunner> CreateSequencedTaskRunner() const override;
  std::unique_ptr<TaskRunner> CreateConcurrentTaskRunner(
      uint32_t concurrent_count) const override;

 private:
  std::unique_ptr<Clock> clock_ = nullptr;
  std::unique_ptr<DeviceInfo> device_info_ = nullptr;
};
}  // namespace fastpair
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_FASTPAIR_COMMON_CONTEXT_IMPL_H_
