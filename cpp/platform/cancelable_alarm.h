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

#ifndef PLATFORM_CANCELABLE_ALARM_H_
#define PLATFORM_CANCELABLE_ALARM_H_

#include <cstdint>

#include "platform/api/lock.h"
#include "platform/api/scheduled_executor.h"
#include "platform/cancelable.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "platform/runnable.h"

namespace location {
namespace nearby {

/**
 * A cancelable alarm with a name. This is a simple wrapper around the logic
 * for posting a Runnable on a ScheduledExecutor and (possibly) later
 * canceling it.
 */
class CancelableAlarm {
 public:
  CancelableAlarm(const std::string& name, Ptr<Runnable> runnable,
                  std::int64_t delay_millis,
                  Ptr<ScheduledExecutor> scheduled_executor);
  ~CancelableAlarm();

  bool cancel();

 private:
  std::string name_;
  ScopedPtr<Ptr<Lock> > lock_;
  Ptr<Cancelable> cancelable_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_CANCELABLE_ALARM_H_
