// Copyright 2022 Google LLC
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

#ifndef LOCATION_NEARBY_CPP_FASTPAIR_INTERNAL_API_TIMER_H_
#define LOCATION_NEARBY_CPP_FASTPAIR_INTERNAL_API_TIMER_H_

#include <functional>

namespace nearby {
namespace api {

class Timer {
 public:
  virtual ~Timer() = default;

  // Creates a timer based on interval.
  //
  // @param delay
  //        The amount of time in milliseconds relative to the current
  //        time that must elapse before the timer is signaled for the first
  //        time.
  // @param interval
  //        The period of the timer, in milliseconds. If this parameter
  //        is zero, the timer is signaled once.
  // @param callback
  //        It will be called when timer signaled.
  // @return
  //        return true if success, otherwise false
  virtual bool Create(int delay, int interval,
                      std::function<void()> callback) = 0;

  // Stops timer. No timer signal is sent after the call.
  virtual bool Stop() = 0;
  virtual bool FireNow() = 0;
};

}  // namespace api
}  // namespace nearby

#endif  // LOCATION_NEARBY_CPP_FASTPAIR_INTERNAL_API_TIMER_H_
