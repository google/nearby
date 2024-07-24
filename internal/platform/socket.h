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

#ifndef PLATFORM_BASE_SOCKET_H_
#define PLATFORM_BASE_SOCKET_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
#include "proto/connections_enums.pb.h"

namespace nearby {

// A socket is an endpoint for communication between two machines.
//
// https://docs.oracle.com/javase/8/docs/api/java/net/Socket.html
class Socket {
 public:
  virtual ~Socket() = default;

  virtual InputStream& GetInputStream() = 0;
  virtual OutputStream& GetOutputStream() = 0;
  virtual Exception Close() = 0;
};

class MediumSocket : public Socket {
 public:
  explicit MediumSocket(location::nearby::proto::connections::Medium medium)
      : medium_(medium) {}
  ~MediumSocket() override = default;

  /** Returns the medium of the socket. */
  virtual location::nearby::proto::connections::Medium GetMedium() const {
    return medium_;
  }

  /** Creates a virtual socket. */
  virtual MediumSocket* CreateVirtualSocket(
      const std::string& salted_service_id_hash_key, OutputStream* outputstream,
      location::nearby::proto::connections::Medium medium,
      absl::flat_hash_map<std::string, std::shared_ptr<MediumSocket>>*
          virtual_sockets_ptr) {
    return this;
  }

  /** Feeds the received incoming data to the client. */
  virtual void FeedIncomingData(ByteArray data) {
//    NEARBY_LOGS(INFO) << "FeedIncomingData: do nothing";
  }

  /** Returns true if the socket is a virtual socket. */
  virtual bool IsVirtualSocket() {
    return false;
  }

  /** Adds a listener to be invoked when the socket is closed. */
  void AddOnSocketClosedListener(
      std::unique_ptr<absl::AnyInvocable<void()>> socket_closed_listener) {
    socket_closed_listeners_.insert(std::move(socket_closed_listener));
  }

  /** Adds a listener to be invoked when the multiplex socket is enabled. */
  void RegisterMultiplexEnabledCallback(
      std::shared_ptr<absl::AnyInvocable<void()>> callback) {
    multiplex_socket_enabled_cbs_.insert(std::move(callback));
  }

  /** Enables the multiplex socket. */
  void EnableMultiplexSocket() {
    if (!IsVirtualSocket()) {
      return;
    }
    for (auto& callback : multiplex_socket_enabled_cbs_) {
      (*callback)();
    }
  }

  /** Closes the local socket. */
  void CloseLocal() {
    for (auto& listener : socket_closed_listeners_) {
      (*listener)();
    }
  }

 private:
  location::nearby::proto::connections::Medium medium_;
  absl::flat_hash_set<std::shared_ptr<absl::AnyInvocable<void()>>>
      socket_closed_listeners_;
  absl::flat_hash_set<std::shared_ptr<absl::AnyInvocable<void()>>>
      multiplex_socket_enabled_cbs_;
};

}  // namespace nearby

#endif  // PLATFORM_BASE_SOCKET_H_
