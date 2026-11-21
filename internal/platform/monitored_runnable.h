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

#ifndef PLATFORM_PUBLIC_MONITORED_RUNNABLE_H_
#define PLATFORM_PUBLIC_MONITORED_RUNNABLE_H_

#include <string>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/platform/implementation/system_clock.h"
#include "internal/platform/runnable.h"

namespace nearby {

// A runnable with extra logging.
// We log if the task has been waiting long on the executor or if it was running
// for a long time. The latter isn't always an issue - some tasks are expected
// to run for longer periods of time (minutes).
class MonitoredRunnable {
 public:
  explicit MonitoredRunnable(Runnable runnable);
  MonitoredRunnable(absl::string_view name, Runnable runnable);

  // Not copyable or movable.
  MonitoredRunnable(const MonitoredRunnable&) = delete;
  MonitoredRunnable& operator=(const MonitoredRunnable&) = delete;

  ~MonitoredRunnable();

  void operator()();

 private:
  std::string name_;
  Runnable runnable_;
  absl::Time post_time_ = SystemClock::ElapsedRealtime();
  bool is_pending_ = true;
};

// Constructs an immovable `MonitoredRunnable` in-place inside a `Runnable`, or
// returns an empty `Runnable` when `runnable` is empty.
Runnable MakeMonitoredRunnable(Runnable runnable);
Runnable MakeMonitoredRunnable(absl::string_view name, Runnable runnable);

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_MONITORED_RUNNABLE_H_
