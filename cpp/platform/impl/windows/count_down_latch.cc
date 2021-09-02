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

#include "platform/impl/windows/count_down_latch.h"

namespace location {
namespace nearby {
namespace windows {

// A synchronization aid that allows one or more threads to wait until a set of
// operations being performed in other threads completes.
//
// Creates or opens a named or unnamed event object.
// https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/CountDownLatch.html
CountDownLatch::CountDownLatch(int count) {
  if (count < 0) {
    throw(std::invalid_argument("count"));
  }
  // https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createeventa
  h_count_down_latch_event_ =
      CreateEvent(NULL,               // default security attributes
                  TRUE,               // manual-reset event
                  FALSE,              // initial state is nonsignaled
                  TEXT("LatchEvent")  // object name
      );
  count_ = count;
}

Exception CountDownLatch::Await() {
  // Waits until the specified object is in the signaled state or the time-out
  // interval elapses.
  // https://docs.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
  if (WaitForSingleObject(h_count_down_latch_event_, INFINITE) ==
      WAIT_OBJECT_0) {
    return Exception{Exception::kSuccess};
  }
  return Exception{Exception::kFailed};
}

ExceptionOr<bool> CountDownLatch::Await(absl::Duration timeout) {
  auto result = WaitForSingleObject(h_count_down_latch_event_,
                                    absl::ToInt64Milliseconds(timeout));
  if (result == WAIT_OBJECT_0) {
    return ExceptionOr<bool>(true);
  }

  if (result == WAIT_TIMEOUT) {
    return ExceptionOr<bool>{Exception::kTimeout};
  }

  return ExceptionOr<bool>{Exception::kFailed};
}

void CountDownLatch::CountDown() {
  // Decrements (decreases by one) the value of the specified 32-bit variable as
  // an atomic operation.
  // https://docs.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-interlockeddecrement
  InterlockedDecrement(&count_);
  if (count_ == 0) {
    SetEvent(h_count_down_latch_event_);
  }
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
