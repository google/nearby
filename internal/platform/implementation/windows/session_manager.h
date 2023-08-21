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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_SESSION_MANAGER_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_SESSION_MANAGER_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "internal/platform/implementation/windows/submittable_executor.h"

namespace nearby {
namespace windows {

// SessionManager provides methods to access/control platform session.
class SessionManager {
 public:
  enum class SessionState { kLock, kUnlock };
  ~SessionManager();

  // Setups session listener.
  // listener_name - Listener name. it should be unique in the SDK level.
  // callback - It will be called when session state changed, such as
  //            lock/unlock screen.
  bool RegisterSessionListener(absl::string_view listener_name,
                               absl::AnyInvocable<void(SessionState)> callback);

  // Removes session listener by its name.
  bool UnregisterSessionListener(absl::string_view listener_name);

  bool IsScreenLocked() const;

  // Prevents the Windows device from sleeping state.
  bool PreventSleep() const;

  // Allows the Windows device to sleep state.
  bool AllowSleep() const;

  static void NotifySessionState(SessionState state);

 private:
  void StartSession(absl::Notification& notification);
  void StopSession();
  void CleanUp();

  absl::flat_hash_set<std::string> listeners_;

  // Static class members
  static absl::Mutex session_mutex_;
  static HWND session_hwnd_;
  static SubmittableExecutor* session_thread_;
  static absl::flat_hash_map<
      std::string, absl::AnyInvocable<void(SessionManager::SessionState)>>*
      session_callbacks_;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_SESSION_MANAGER_H_
