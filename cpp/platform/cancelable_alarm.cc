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

#include "platform/cancelable_alarm.h"

#include "platform/api/platform.h"
#include "platform/api/scheduled_executor.h"
#include "platform/synchronized.h"

namespace location {
namespace nearby {

namespace {
using Platform = platform::ImplementationPlatform;
}

CancelableAlarm::CancelableAlarm(const std::string &name,
                                 Ptr<Runnable> runnable,
                                 std::int64_t delay_millis,
                                 Ptr<ScheduledExecutor> scheduled_executor)
    : name_(name),
      lock_(Platform::createLock()),
      cancelable_(scheduled_executor->schedule(runnable, delay_millis)) {}

CancelableAlarm::~CancelableAlarm() { cancelable_.destroy(); }

bool CancelableAlarm::cancel() {
  Synchronized s(lock_.get());

  if (cancelable_.isNull()) {
    // TODO(tracyzhou): Add logging
    return false;
  }

  bool canceled = cancelable_->cancel();
  // TODO(tracyzhou): Add logging
  cancelable_.destroy();
  return canceled;
}

}  // namespace nearby
}  // namespace location
