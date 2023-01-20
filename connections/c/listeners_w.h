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

#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_C_LISTENERS_W_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_C_LISTENERS_W_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

// This file defines all the protocol listeners and their parameter structures.
// Listeners are defined as collections of std::function<T> instances, which is
// more flexible than a virtual function:
// - a subset of listener callbacks may be overridden, while others may remain
//   default-initialized.
// - callbacks may be initialized with lambdas; lambda definitions are concize.

#include "connections/c/medium_selector_w.h"
#include "connections/c/payload_w.h"
#include "connections/status.h"
#include "internal/platform/payload_id.h"

namespace nearby {
// Forward declarations
namespace connections {
struct ConnectionListener;
struct DLL_API ConnectionListenerDeleter {
  void operator()(connections::ConnectionListener* p);
};

struct DiscoveryListener;
struct DLL_API DiscoveryListenerDeleter {
  void operator()(connections::DiscoveryListener* p);
};

struct PayloadListener;
struct DLL_API PayloadListenerDeleter {
  void operator()(connections::PayloadListener* p);
};

struct ResultCallback;
struct ResultCallbackDeleter {
  void operator()(connections::ResultCallback* p);
};

struct ConnectionResponseInfo;
struct PayloadProgressInfo;
}  // namespace connections

namespace windows {

using ::nearby::connections::Status;

template <class T>
T DefaultConstructor() {
  return T();
}
template <class T>
void DefaultConstructor(T t) {}
template <class T, class C>
void DefaultConstructor(T t, C c) {}
template <class T, class C, class D>
void DefaultConstructor(T t, C c, size_t size, D d) {}

extern "C" {

// Common callback for asynchronously invoked methods.
// Called after a job scheduled for execution is completed.
// This is not the same as completion of the associated process,
// which may have many states, and multiple async jobs, and be still ongoing.
// Progress on the overall process is reported by the associated listener.
struct DLL_API ResultCallbackW {
  // Callback to access the status of the operation when available.
  // status - result of job execution;
  //   Status::kSuccess, if successful; anything else indicates failure.
  ResultCallbackW();
  ~ResultCallbackW();
  ResultCallbackW(ResultCallbackW& other);
  ResultCallbackW(ResultCallbackW&& other) noexcept;

  void (*result_cb)(Status status) = DefaultConstructor;

  std::unique_ptr<connections::ResultCallback,
                  connections::ResultCallbackDeleter>
  GetImpl() {
    return std::move(impl_);
  }

 private:
  std::unique_ptr<connections::ResultCallback,
                  connections::ResultCallbackDeleter>
      impl_;
};

struct DLL_API ConnectionResponseInfoW {
  const char* remote_endpoint_info;
  size_t remote_endpoint_info_size;
  const char* authentication_token;
  const char* raw_authentication_token;
  size_t raw_authentication_token_size;
  bool is_incoming_connection = false;
  bool is_connection_verified = false;
};

struct DLL_API PayloadProgressInfoW {
  PayloadId payload_id = 0;
  enum class Status {
    kSuccess,
    kFailure,
    kInProgress,
    kCanceled,
  } status = Status::kSuccess;
  size_t total_bytes = 0;
  size_t bytes_transferred = 0;
};

enum class DLL_API DistanceInfoW {
  kUnknown = 1,
  kVeryClose = 2,
  kClose = 3,
  kFar = 4,
};

struct DLL_API ConnectionListenerW {
  typedef void (*InitiatedCB)(const char* endpoint_id,
                              const ConnectionResponseInfoW& info);

  typedef void (*AcceptedCB)(const char* endpoint_id);
  typedef void (*RejectedCB)(const char* endpoint_id, Status status);

  typedef void (*DisconnectedCB)(const char* endpoint_id);
  typedef void (*BandwidthChangedCB)(const char* endpoint_id, MediumW medium);

  ConnectionListenerW(InitiatedCB, AcceptedCB, RejectedCB, DisconnectedCB,
                      BandwidthChangedCB);
  ConnectionListenerW(ConnectionListenerW& other);
  ConnectionListenerW(ConnectionListenerW&& other) noexcept;

  // A basic encrypted channel has been created between you and the endpoint.
  // Both sides are now asked if they wish to accept or reject the connection
  // before any data can be sent over this channel.
  //
  // This is your chance, before you accept the connection, to confirm that you
  // connected to the correct device. Both devices are given an identical token;
  // it's up to you to decide how to verify it before proceeding. Typically this
  // involves showing the token on both devices and having the users manually
  // compare and confirm; however, this is only required if you desire a secure
  // connection between the devices.
  //
  // Whichever route you decide to take (including not authenticating the other
  // device), call Core::AcceptConnection() when you're ready to talk, or
  // Core::RejectConnection() to close the connection.
  //
  // endpoint_id - The identifier for the remote endpoint.
  // info -  Other relevant information about the connection.
  InitiatedCB initiated_cb = DefaultConstructor;

  // Called after both sides have accepted the connection.
  // Both sides may now send Payloads to each other.
  // Call Core::SendPayload() or wait for incoming PayloadListener::OnPayload().
  //
  // endpoint_id - The identifier for the remote endpoint.
  AcceptedCB accepted_cb = DefaultConstructor;

  // Called when either side rejected the connection.
  // Payloads can not be exchanged. Call Core::DisconnectFromEndpoint()
  // to terminate connection.
  //
  // endpoint_id - The identifier for the remote endpoint.
  RejectedCB rejected_cb = DefaultConstructor;

  // Called when a remote endpoint is disconnected or has become unreachable.
  // At this point service (re-)discovery may start again.
  //
  // endpoint_id - The identifier for the remote endpoint.
  DisconnectedCB disconnected_cb = DefaultConstructor;

  // Called when the connection's available bandwidth has changed.
  //
  // endpoint_id - The identifier for the remote endpoint.
  // medium      - Medium we upgraded to.
  BandwidthChangedCB bandwidth_changed_cb = DefaultConstructor;

  std::unique_ptr<connections::ConnectionListener,
                  connections::ConnectionListenerDeleter>
  GetImpl() {
    return std::move(impl_);
  }

 private:
  std::unique_ptr<connections::ConnectionListener,
                  connections::ConnectionListenerDeleter>
      impl_;
};

struct DLL_API DiscoveryListenerW {
  typedef void (*EndpointFoundCB)(const char* endpoint_id,
                                  const char* endpoint_info,
                                  size_t endpoint_info_size,
                                  const char* service_id);
  typedef void (*EndpointLostCB)(const char* endpoint_id);
  typedef void (*EndpointDistanceChangedCB)(const char* endpoint_id,
                                            DistanceInfoW info);

  DiscoveryListenerW(EndpointFoundCB endpointFoundCB,
                     EndpointLostCB endpointLostCB,
                     EndpointDistanceChangedCB endpointDistanceChangedCB);
  DiscoveryListenerW(DiscoveryListenerW& other);
  DiscoveryListenerW(DiscoveryListenerW&& other) noexcept;

  // Called when a remote endpoint is discovered.
  //
  // endpoint_id   - The ID of the remote endpoint that was discovered.
  // endpoint_info - The info of the remote endpoint represented by ByteArray.
  // service_id    - The ID of the service advertised by the remote endpoint.
  EndpointFoundCB endpoint_found_cb = DefaultConstructor;

  // Called when a remote endpoint is no longer discoverable; only called for
  // endpoints that previously had been passed to {@link
  // #onEndpointFound(String, DiscoveredEndpointInfo)}.
  //
  // endpoint_id - The ID of the remote endpoint that was lost.
  EndpointLostCB endpoint_lost_cb = DefaultConstructor;

  // Called when a remote endpoint is found with an updated distance.
  //
  // arguments:
  //   endpoint_id - The ID of the remote endpoint that was lost.
  //   info        - The distance info, encoded as enum value.
  EndpointDistanceChangedCB endpoint_distance_changed_cb = DefaultConstructor;

  std::unique_ptr<connections::DiscoveryListener,
                  connections::DiscoveryListenerDeleter>
  GetImpl() {
    return std::move(impl_);
  }

 private:
  std::unique_ptr<connections::DiscoveryListener,
                  connections::DiscoveryListenerDeleter>
      impl_;
};

struct DLL_API PayloadListenerW {
  typedef void (*PayloadCB)(const char* endpoint_id, PayloadW& payload);
  typedef void (*PayloadProgressCB)(const char* endpoint_id,
                                    const PayloadProgressInfoW& info);

  PayloadListenerW(PayloadCB, PayloadProgressCB);
  PayloadListenerW(PayloadListenerW& other);
  PayloadListenerW(PayloadListenerW&& other) noexcept;

  // Called when a Payload is received from a remote endpoint. Depending
  // on the type of the Payload, all of the data may or may not have been
  // received at the time of this call. Use OnPayloadProgress() to
  // get updates on the status of the data received.
  //
  // endpoint_id - The identifier for the remote endpoint that sent the
  //               payload.
  // payload     - The Payload object received.
  PayloadCB payload_cb = DefaultConstructor;

  // Called with progress information about an active Payload transfer, either
  // incoming or outgoing.
  //
  // endpoint_id - The identifier for the remote endpoint that is sending or
  //               receiving this payload.
  // info -  The PayloadProgressInfo structure describing the status of
  //         the transfer.
  PayloadProgressCB payload_progress_cb = DefaultConstructor;

  std::unique_ptr<connections::PayloadListener,
                  connections::PayloadListenerDeleter>
  GetImpl() {
    return std::move(impl_);
  }

 private:
  std::unique_ptr<connections::PayloadListener,
                  connections::PayloadListenerDeleter>
      impl_;
};

}  // extern "C"
}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_C_LISTENERS_W_H_
