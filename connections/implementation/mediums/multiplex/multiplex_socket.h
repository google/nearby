// Copyright 2024 Google LLC
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

#ifndef CORE_INTERNAL_MEDIUMS_MULTIPLEX_MULTIPLEX_SOCKET_H_
#define CORE_INTERNAL_MEDIUMS_MULTIPLEX_MULTIPLEX_SOCKET_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "connections/implementation/mediums/multiplex/multiplex_output_stream.h"
#include "connections/medium_selector.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/future.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex.h"
#include "internal/platform/single_thread_executor.h"
#include "internal/platform/socket.h"
#include "proto/connections_enums.pb.h"
#include "proto/mediums/multiplex_frames.pb.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace multiplex {

using MultiplexEnbaleCb = absl::AnyInvocable<void()>;
using MultiplexIncomingConnectionCb = absl::AnyInvocable<void(
    const std::string& service_id, MediumSocket* socket)>;

class MultiplexSocket {
 public:
  MultiplexSocket(const MultiplexSocket&) = delete;
  MultiplexSocket& operator=(const MultiplexSocket&) = delete;
  ~MultiplexSocket() { ShutdownAll(); };

  // Creates a new incoming MultiplexSocket.
  static MultiplexSocket* CreateIncomingSocket(
      std::shared_ptr<MediumSocket> physical_socket,
      const std::string& service_id, std::int32_t first_frame_len);
  // Creates a new outgoing MultiplexSocket.
  static MultiplexSocket* CreateOutgoingSocket(
      std::shared_ptr<MediumSocket> physical_socket,
      const std::string& service_id, const std::string& service_id_hash_salt);

  // Creates a new outgoing MultiplexSocket with default service_id_hash_salt.
  static MultiplexSocket* CreateOutgoingSocket(
      std::shared_ptr<MediumSocket> physical_socket,
      const std::string& service_id);

  // A Table of service Id as row key, medium type as column key, and
  // MultiplexIncomingConnectionCb as value. Non-empty while the client starts
  // listening for incoming virtual socket. The MultiplexIncomingConnectionCb
  // will be called when the incoming virtual socket is established.
  static absl::flat_hash_map<
      std::pair<std::string, ::location::nearby::proto::connections::Medium>,
      MultiplexIncomingConnectionCb>&
  GetIncomingConnectionCallbacks();

  // Listens for incoming connection through multiplex for specified {@code
  // service_id} on medium
  // {@code type}. Should register the callback before new the MultiplexSocket.
  static void ListenForIncomingConnection(
      const std::string& service_id,
      ::location::nearby::proto::connections::Medium type,
      absl::AnyInvocable<void(const std::string& service_id,
                              MediumSocket* socket)>
          incoming_connection_cb);

  // Stops listening for incoming multiplex connection for {@code service_id} on
  // medium {@code type}.
  static void StopListeningForIncomingConnection(
      const std::string& service_id,
      ::location::nearby::proto::connections::Medium type);

  bool IsEnabled() { return enabled_.Get(); }
  void Enable() {
    NEARBY_LOGS(INFO) << "Enable the Multiplex MediumSocket.";
    enabled_.Set(true);
  }

  // Gets the virtual socket by service id.
  MediumSocket* GetVirtualSocket(const std::string& service_id);
  // Gets the virtual socket count.
  int GetVirtualSocketCount();

  void ListVirtualSocket();

  // Establishes the virtual socket by service id.
  MediumSocket* EstablishVirtualSocket(const std::string& service_id);
  // Shuts down the multiplex socket.
  void Shutdown();
  bool IsShutdown() { return is_shutdown_; }
  void SetShutdown(bool is_shutdown) { is_shutdown_ = is_shutdown; }
  void ShutdownAll();

 private:
  explicit MultiplexSocket(std::shared_ptr<MediumSocket> physical_socket);

  // Creates the first virtual socket for the service id. The first virtual
  // socket is created by the sender.
  MediumSocket* CreateFirstVirtualSocket(
      const std::string& service_id, const std::string& service_id_hash_salt);
  // Creates the virtual socket for the service id.
  MediumSocket* CreateVirtualSocket(const std::string& service_id,
                                    const std::string& service_id_hash_salt);
  // Registers the connection response future for the service id.
  std::shared_ptr<Future<::location::nearby::mediums::ConnectionResponseFrame::
                             ConnectionResponseCode>>
  RegisterConnectionResponse(const std::string& service_id);
  // Unregisters the connection response future for the service id.
  void UnRegisterConnectionResponse(const std::string& service_id);
  // Starts the reader thread to read the incoming MultiplexFrame from the
  // physical socket.
  void StartReaderThread(std::int32_t first_frame_len);
  // Handles the offline frame from the physical socket.
  void HandleOfflineFrame(const ByteArray& bytes);
  // Handles the control frame from the physical socket.
  void HandleControlFrame(
      const ByteArray& salted_service_id_hash,
      const std::string& service_id_hash_salt,
      const ::location::nearby::mediums::MultiplexControlFrame& frame);
  // Handles the connection request frame from the physical socket.
  void HandleConnectionRequest(const ByteArray& salted_service_id_hash,
                               const std::string& service_id_hash_salt);
  // Handles the connection response frame from the physical socket.
  void HandleConnectionResponse(
      const ByteArray& salted_service_id_hash,
      const std::string& service_id_hash_salt,
      const ::location::nearby::mediums::ConnectionResponseFrame& frame);
  // Handles the disconnection frame from the physical socket.
  void HandleDisconnection(const ByteArray& salted_service_id_hash);
  // Handles the data frame from the physical socket.
  void HandleDataFrame(
      const ByteArray& salted_service_id_hash,
      const std::string& service_id_hash_salt,
      const ::location::nearby::mediums::MultiplexDataFrame& frame);
  // Handles the physical socket closed.
  void OnPhysicalSocketClosed();
  // Remaps and gets the virtual socket by service id hash.
  MediumSocket* ReMapAndGetVirtualSocket(
      const ByteArray& salted_service_id_hash,
      const std::string& service_id_hash_salt);
  // Handles the virtual socket closed.
  void OnVirtualSocketClosed(const std::string& service_id);
  // Runs the offload thread.
  void RunOffloadThread(const std::string& name,
                        absl::AnyInvocable<void()> runnable);

  // The physical socket connect to the remote device.
  std::shared_ptr<MediumSocket> physical_socket_ptr_;

  // The output stream to manage all outgoing frames from all clients.
  MultiplexOutputStream multiplex_output_stream_;
  // The {@link InputStream} of the physical socket. It is used to read the
  // incoming MultiplexFrame from the physical socket.
  InputStream* physical_reader_;
  // The medium type of the physical socket.
  Medium medium_;

  // The callback to enable the MultiplexSocket.
  std::shared_ptr<absl::AnyInvocable<void()>> enable_cb_ =
      std::make_shared<absl::AnyInvocable<void()>>([this]() { Enable(); });

  // A map of service Id -> {@link SettableFuture} for waiting the
  // ConnectionResponse. Non-empty while requesting the virtual socket.
  absl::flat_hash_map<std::string,
                      std::shared_ptr<Future<
                          ::location::nearby::mediums::ConnectionResponseFrame::
                              ConnectionResponseCode>>>
      connection_response_futures_;

  // A map of service Id hash key -> virtual socket. Non-empty while at least
  // one virtual socket alive. Class derived from "MediumSocket" should define a
  // pointer to the virtual sockets map. When here's any virtual socket
  // operation, it will be reflected in both derived MediumSocket class and
  // MultiplexSocket object
  mutable Mutex virtual_socket_mutex_;
  absl::flat_hash_map<std::string, std::shared_ptr<MediumSocket>>
      // virtual_sockets_ ABSL_GUARDED_BY(virtual_socket_mutex_);
      virtual_sockets_;

  // The thread to receive incoming MultiplexFrame from the physical socket.
  SingleThreadExecutor physical_reader_thread_;
  // The single thread we throw the potentially blocking work on to.
  SingleThreadExecutor single_thread_offloader_;

  // The status of the MultiplexSocket enabled or disabled, it depends on both
  // Sender and Receiver supports MultiplexSocket or not. Default disabled and
  // enable it once two devices negotiated finished.
  AtomicBoolean enabled_{false};

  // If the socket is already shutdown and no longer in use.
  bool is_shutdown_ = false;
  static AtomicBoolean is_shutting_down_;
  std::unique_ptr<CountDownLatch> reader_thread_shutdown_barrier_;
};

}  // namespace multiplex
}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_MULTIPLEX_MULTIPLEX_SOCKET_H_
