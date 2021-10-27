// Copyright 2021 Google LLC
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

#ifndef WINDOWS_LISTENERS_H_
#define WINDOWS_LISTENERS_H_

#include <functional>

#include "platform/base/byte_array.h"
#include "platform/base/byte_utils.h"
#include "platform/public/core_config.h"

namespace location {
namespace nearby {
namespace proto {
namespace connections {
enum Medium : int;
}
}  // namespace proto
namespace connections {
struct ResultCallback;
struct ConnectionResponseInfo;
struct ConnectionListener;
struct DiscoveryListener;
struct PayloadListener;
class Payload;
enum class DistanceInfo;
struct PayloadProgressInfo;
struct ConnectionRequestInfo;
struct Status;
struct ConnectionResponseInfoFP;
}  // namespace connections

namespace windows {

using Medium = location::nearby::proto::connections::Medium;

// Provides default-initialization with a valid empty method,
// instead of nullptr. This allows partial initialization
// of a set of listeners.
template <typename Function>
struct function_traits;

template <typename Ret, typename... Args>
struct function_traits<Ret(Args...)> {
  typedef Ret (*ptr)(Args...);
};

template <typename Ret, typename... Args>
struct function_traits<Ret (*const)(Args...)> : function_traits<Ret(Args...)> {
};

template <typename Cls, typename Ret, typename... Args>
struct function_traits<Ret (Cls::*)(Args...) const>
    : function_traits<Ret(Args...)> {};

using voidfun = void (*)();

template <typename F>
voidfun make_voidfun_callback(F lambda) {
  static auto lambda_copy = lambda;

  return []() { lambda_copy(); };
}

template <typename F>
auto make_callback(F lambda) ->
    typename function_traits<decltype(&F::operator())>::ptr {
  static auto lambda_copy = lambda;

  return ([]<typename... Args>(Args... args) { return lambda_copy(args...); });
}

// The following template can be used in place of
// the specific default function pointers declared
// in the body of this code, once c++20 is available
// requires C++20
// template <typename... Args>
// constexpr void (*DefaultFPCallback)(Args...){};

constexpr void (*DefaultResultFPCallback)(connections::Status){};  // NOLINT

// Common callback for asynchronously invoked methods.
// Called after a job scheduled for execution is completed.
// This is not the same as completion of the associated process,
// which may have many states, and multiple async jobs, and be still ongoing.
// Progress on the overall process is reported by the associated listener.
extern "C" struct DLL_API ResultCallbackFP {
  // Callback to access the status of the operation when available.
  // status - result of job execution;
  //   Status::kSuccess, if successful; anything else indicates failure.
  void(__stdcall* result_cb)(connections::Status) = DefaultResultFPCallback;

  __stdcall ResultCallbackFP();

 private:
  std::unique_ptr<connections::ResultCallback,
                  void (*)(connections::ResultCallback*)>
      impl_;
};

extern "C" struct DLL_API ConnectionResponseInfoFP {
  ConnectionResponseInfoFP(const connections::ConnectionResponseInfo* other);

  operator connections::ConnectionResponseInfo() const;

  const char* GetAuthenticationDigits();

  ByteArray remote_endpoint_info;
  std::string authentication_token;
  ByteArray raw_authentication_token;
  bool is_incoming_connection = false;
  bool is_connection_verified = false;
  char four_digit_char_string_[FOUR_DIGIT_STRING_SIZE];
};

constexpr void(__stdcall* DefaultConnectionListenerInitiatedFPCallback)(
    const char*, const ConnectionResponseInfoFP&){};  // NOLINT
constexpr void(__stdcall* DefaultConnectionListenerAcceptedFPCallback)(
    const char*){};  // NOLINT
constexpr void(__stdcall* DefaultConnectionListenerRejectedFPCallback)(
    const char*, connections::Status){};  // NOLINT
constexpr void(__stdcall* DefaultConnectionListenerDisconnectedFPCallback)(
    const char*){};  // NOLINT
constexpr void(__stdcall* DefaultConnectionListenerBandwidthChangedFPCallback)(
    const char*, Medium medium){};  // NOLINT

extern "C" struct DLL_API ConnectionListenerFP {
  ConnectionListenerFP();
  ConnectionListenerFP(connections::ConnectionListener connection_listener);
  operator connections::ConnectionListener() const;

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
  // connection_response_info -  Other relevant information about the
  // connection.
  void(__stdcall* initiated_cb)(
      const char* endpoint_id,
      const ConnectionResponseInfoFP& connection_response_info) =
      DefaultConnectionListenerInitiatedFPCallback;

  // Called after both sides have accepted the connection.
  // Both sides may now send Payloads to each other.
  // Call Core::SendPayload() or wait for incoming PayloadListener::OnPayload().
  //
  // endpoint_id - The identifier for the remote endpoint.
  void(__stdcall* accepted_cb)(const char* endpoint_id) =
      DefaultConnectionListenerAcceptedFPCallback;

  // Called when either side rejected the connection.
  // Payloads can not be exchaged. Call Core::DisconnectFromEndpoint()
  // to terminate connection.
  //
  // endpoint_id - The identifier for the remote endpoint.
  void(__stdcall* rejected_cb)(const char* endpoint_id,
                               connections::Status status) =
      DefaultConnectionListenerRejectedFPCallback;

  // Called when a remote endpoint is disconnected or has become unreachable.
  // At this point service (re-)discovery may start again.
  //
  // endpoint_id - The identifier for the remote endpoint.
  void(__stdcall* disconnected_cb)(const char* endpoint_id) =
      DefaultConnectionListenerDisconnectedFPCallback;

  // Called when the connection's available bandwidth has changed.
  //
  // endpoint_id - The identifier for the remote endpoint.
  // medium      - Medium we upgraded to.
  void(__stdcall* bandwidth_changed_cb)(const char* endpoint_id,
                                        Medium medium) =
      DefaultConnectionListenerBandwidthChangedFPCallback;

 private:
  std::unique_ptr<connections::ConnectionListener,
                  void (*)(connections::ConnectionListener*)>
      impl_;
};

constexpr void(__stdcall* DefaultDiscoveryListenerEndpointFoundFPCallback)(
    const char*, const ByteArray&, const char*){};  // NOLINT
constexpr void(__stdcall* DefaultDiscoveryListenerEndpointLostFPCallback)(
    const char*){};  // NOLINT
constexpr void(
    __stdcall* DefaultDiscoveryListenerEndpointDistanceChangedFPCallback)(
    const char*, connections::DistanceInfo){};  // NOLINT

extern "C" struct DLL_API DiscoveryListenerFP {
  DiscoveryListenerFP();
  operator connections::DiscoveryListener() const;

  // Called when a remote endpoint is discovered.
  //
  // endpoint_id   - The ID of the remote endpoint that was discovered.
  // endpoint_info - The info of the remote endpoint representd by ByteArray.
  // service_id    - The ID of the service advertised by the remote endpoint.
  void(__stdcall* endpoint_found_cb)(
      const char* endpoint_id, const ByteArray& endpoint_info,
      const char* service_id) = DefaultDiscoveryListenerEndpointFoundFPCallback;

  // Called when a remote endpoint is no longer discoverable; only called for
  // endpoints that previously had been passed to {@link
  // #onEndpointFound(String, DiscoveredEndpointInfo)}.
  //
  // endpoint_id - The ID of the remote endpoint that was lost.
  void(__stdcall* endpoint_lost_cb)(const char* endpoint_id) =
      DefaultDiscoveryListenerEndpointLostFPCallback;

  // Called when a remote endpoint is found with an updated distance.
  //
  // arguments:
  //   endpoint_id - The ID of the remote endpoint that was lost.
  //   distance_info - The distance info, encoded as enum value.
  void(__stdcall* endpoint_distance_changed_cb)(
      const char* endpoint_id, connections::DistanceInfo distance_info) =
      DefaultDiscoveryListenerEndpointDistanceChangedFPCallback;

 private:
  std::unique_ptr<connections::DiscoveryListener,
                  void (*)(connections::DiscoveryListener*)>
      impl_;
};

constexpr void(__stdcall* DefaultPayloadListenerPayloadFPCallback)(
    const char*, connections::Payload&){};  // NOLINT
constexpr void(__stdcall* DefaultPayloadListenerPayloadProgressFPCallback)(
    const char*, const connections::PayloadProgressInfo&){};  // NOLINT

extern "C" struct DLL_API PayloadListenerFP {
  PayloadListenerFP();
  operator connections::PayloadListener() const;

  // Called when a Payload is received from a remote endpoint. Depending
  // on the type of the Payload, all of the data may or may not have been
  // received at the time of this call. Use OnPayloadProgress() to
  // get updates on the status of the data received.
  //
  // endpoint_id - The identifier for the remote endpoint that sent the
  //               payload.
  // payload     - The Payload object received.
  void(__stdcall* payload_cb)(const char* endpoint_id,
                              connections::Payload& payload) =
      DefaultPayloadListenerPayloadFPCallback;

  // Called with progress information about an active Payload transfer, either
  // incoming or outgoing.
  //
  // endpoint_id - The identifier for the remote endpoint that is sending or
  //               receiving this payload.
  // payload_progress_info - The PayloadProgressInfo structure describing the
  // status of
  //         the transfer.
  void(__stdcall* payload_progress_cb)(
      const char* endpoint_id,
      const connections::PayloadProgressInfo& payload_progress_info) =
      DefaultPayloadListenerPayloadProgressFPCallback;

 private:
  std::unique_ptr<connections::PayloadListener,
                  void (*)(connections::PayloadListener*)>
      impl_;
};

extern "C" struct DLL_API ConnectionRequestInfoFP {
  operator connections::ConnectionRequestInfo() const;
  __stdcall ConnectionRequestInfoFP(
      const connections::ConnectionRequestInfo* connection_request_info);

  ByteArray endpoint_info;
  ConnectionListenerFP listener;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location

#endif  // WINDOWS_LISTENERS_H_
