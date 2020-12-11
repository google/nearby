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

#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_H_

#include <memory>
#include <string>

#include "core/internal/mediums/webrtc/connection_flow.h"
#include "core/internal/mediums/webrtc/data_channel_listener.h"
#include "core/internal/mediums/webrtc/local_ice_candidate_listener.h"
#include "core/internal/mediums/webrtc/peer_id.h"
#include "core/internal/mediums/webrtc/webrtc_socket.h"
#include "core/internal/mediums/webrtc/webrtc_socket_wrapper.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform/base/byte_array.h"
#include "platform/base/listeners.h"
#include "platform/base/runnable.h"
#include "platform/public/atomic_boolean.h"
#include "platform/public/cancelable_alarm.h"
#include "platform/public/future.h"
#include "platform/public/mutex.h"
#include "platform/public/scheduled_executor.h"
#include "platform/public/single_thread_executor.h"
#include "platform/public/webrtc.h"
#include "location/nearby/mediums/proto/web_rtc_signaling_frames.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/jsep.h"
#include "webrtc/api/scoped_refptr.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

// Callback that is invoked when a new connection is accepted.
struct AcceptedConnectionCallback {
  std::function<void(WebRtcSocketWrapper socket)> accepted_cb =
      DefaultCallback<WebRtcSocketWrapper>();
};

// Entry point for connecting a data channel between two devices via WebRtc.
class WebRtc {
 public:
  WebRtc();
  ~WebRtc();

  // Gets the default two-letter country code associated with current locale.
  // For example, en_US locale resolves to "US".
  const std::string GetDefaultCountryCode();

  // Returns if WebRtc is available as a medium for nearby to transport data.
  // Runs on @MainThread.
  bool IsAvailable();

  // Returns if the device is accepting connection with specific service id.
  // Runs on @MainThread.
  bool IsAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Prepares the device to accept incoming WebRtc connections. Returns a
  // boolean value indicating if the device has started accepting connections.
  // Runs on @MainThread.
  bool StartAcceptingConnections(const std::string& service_id,
                                 const PeerId& self_id,
                                 const LocationHint& location_hint,
                                 AcceptedConnectionCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Try to stop (accepting) the specific connection with provided service id.
  // Runs on @MainThread
  void StopAcceptingConnections(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Initiates a WebRtc connection with peer device identified by |peer_id|.
  // Runs on @MainThread.
  WebRtcSocketWrapper Connect(const PeerId& peer_id,
                              const LocationHint& location_hint)
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  enum class Role {
    kNone = 0,
    kOfferer = 1,
    kAnswerer = 2,
  };

  absl::flat_hash_map<Role, std::string> role_names_{
      {Role::kNone, "None"},
      {Role::kOfferer, "Offerer"},
      {Role::kAnswerer, "Answerer"}};

  struct ConnectionInfo {
    std::unique_ptr<ConnectionFlow> connection_flow;
    std::unique_ptr<WebRtcSignalingMessenger> signaling_messenger;
    WebRtcSocketWrapper socket;
    CancelableAlarm restart_receive_messages_alarm;

    PeerId self_id;
    PeerId peer_id;
    ByteArray pending_local_offer;
    std::vector<::location::nearby::mediums::IceCandidate>
        pending_local_ice_candidates;
    std::string ToString() const;
  };

  bool InitWebRtcFlow(const Role& role, const PeerId& self_id,
                      const LocationHint& location_hint,
                      const std::string& connection_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Future<WebRtcSocketWrapper> ListenForWebRtcSocketFuture(
      const Role& role, const std::string& connection_id,
      Future<rtc::scoped_refptr<webrtc::DataChannelInterface>>
          data_channel_future,
      AcceptedConnectionCallback callback);

  WebRtcSocketWrapper CreateWebRtcSocketWrapper(
      const Role& role, const std::string& connection_id,
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

  LocalIceCandidateListener GetLocalIceCandidateListener(
      const Role& role, const std::string& connection_id);
  void OnLocalIceCandidate(
      const Role& role, const std::string& connection_id,
      const webrtc::IceCandidateInterface* local_ice_candidate);

  DataChannelListener GetDataChannelListener(const Role& role,
                                             const std::string& connection_id);
  void OnDataChannelClosed(const Role& role, const std::string& connection_id);
  void OnDataChannelMessageReceived(const Role& role,
                                    const std::string& connection_id,
                                    const ByteArray& message);
  void OnDataChannelBufferedAmountChanged(const Role& role,
                                          const std::string& connection_id);

  // Runs on @MainThread and |single_thread_executor_|.
  bool SetLocalSessionDescription(SessionDescriptionWrapper sdp, Role role,
                                  const std::string& connection_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  bool IsSignaling(const Role& role, const std::string& connection_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  void ProcessSignalingMessage(const Role& role,
                               const std::string& connection_id,
                               const ByteArray& message)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Runs on |single_thread_executor_|.
  void SendOfferAndIceCandidatesToPeer(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  void SendAnswerToPeer(const std::string& peer_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on @MainThread and |single_thread_executor_|.
  void LogAndDisconnect(const Role& role, const std::string& connection_id,
                        const std::string& error_message)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on @MainThread.
  void Disconnect(const Role& role, const std::string& connection_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Runs on @MainThread and |single_thread_executor_|.
  void DisconnectLocked(const Role& role, const std::string& connection_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void LogAndShutdownSignaling(const Role& role,
                               const std::string& connection_id,
                               const std::string& error_message)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on @MainThread and |single_thread_executor_|.
  void ShutdownSignaling(const Role& role, const std::string& connection_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on @MainThread and |single_thread_executor_|.
  void ShutdownWebRtcSocket(const Role& role, const std::string& connection_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on @MainThread and |single_thread_executor_|.
  void ShutdownIceCandidateCollection(const Role& role,
                                      const std::string& connection_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void OffloadFromSignalingThread(Runnable runnable);

  // Runs on |restart_receive_messages_executor_|.
  void RestartReceiveMessages(const LocationHint& location_hint,
                              const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void PrintStatus(const std::string& func);

  ConnectionInfo* GetConnectionInfo(const Role& role,
                                    const std::string& connection_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  std::string InternalStatesToString() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Mutex mutex_;

  WebRtcMedium medium_;

  SingleThreadExecutor single_thread_executor_;

  // Restarts the signaling messenger for receiving messages.
  ScheduledExecutor restart_receive_messages_executor_;

  // Use service_id as key for accepting connections.
  absl::flat_hash_map<std::string, ConnectionInfo> accepting_map_
      ABSL_GUARDED_BY(mutex_);
  // Use remote peer_id as key for connecting connections.
  absl::flat_hash_map<std::string, ConnectionInfo> connecting_map_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_H_
