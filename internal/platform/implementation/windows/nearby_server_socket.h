// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_NEARBY_SERVER_SOCKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_NEARBY_SERVER_SOCKET_H_

#include <winsock2.h>

#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/implementation/windows/nearby_client_socket.h"
#include "internal/platform/implementation/windows/socket_address.h"

namespace nearby::windows {

class NearbyServerSocket {
 public:
  NearbyServerSocket();
  ~NearbyServerSocket();

  bool Listen(const SocketAddress& address);
  std::unique_ptr<NearbyClientSocket> Accept();
  bool Close();

  int GetPort() const { return port_; }

  // Sets a callback to be called when the socket is closed.
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier);

 private:
  mutable absl::Mutex mutex_;
  bool is_socket_initiated_ = false;
  SOCKET socket_ ABSL_GUARDED_BY(mutex_) = INVALID_SOCKET;
  int port_ = 0;
  absl::AnyInvocable<void()> close_notifier_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_NEARBY_SERVER_SOCKET_H_
