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

#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_IMPL_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "connections/implementation/bwu_handler.h"
#include "connections/implementation/mediums/webrtc.h"
#include "connections/implementation/mediums/webrtc/connection_flow.h"
#include "connections/implementation/mediums/webrtc/session_description_wrapper.h"
#include "connections/implementation/mediums/webrtc/webrtc.h"
#include "connections/implementation/mediums/webrtc_peer_id.h"
#include "connections/implementation/mediums/webrtc_socket.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/expected.h"
#include "internal/platform/future.h"
#include "internal/platform/mutex.h"
#include "internal/platform/runnable.h"
#include "internal/platform/scheduled_executor.h"
#include "proto/mediums/web_rtc_signaling_frames.pb.h"
#include "webrtc/api/jsep.h"
#include "webrtc/rtc_base/network_constants.h"

namespace nearby {
namespace connections {
namespace mediums {

// Entry point for connecting a data channel between two devices via WebRtc.
class WebRtcImpl : public WebRtc {
 public:
  WebRtcImpl();
  ~WebRtcImpl() override;

  // Overrides for WebRtc:
  bool IsAvailable() override;
  bool IsAcceptingConnections(const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool StartAcceptingConnections(
      const std::string& service_id, const WebrtcPeerId& self_peer_id,
      const location::nearby::connections::LocationHint& location_hint,
      AcceptedConnectionCallback callback, bool non_cellular) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  void StopAcceptingConnections(const std::string& service_id) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  ErrorOr<std::shared_ptr<WebRtcSocket>> Connect(
      const std::string& service_id, const WebrtcPeerId& peer_id,
      const location::nearby::connections::LocationHint& location_hint,
      CancellationFlag* cancellation_flag, bool non_cellular) override
      ABSL_LOCKS_EXCLUDED(mutex_);
  bool IsUsingCellular() override ABSL_LOCKS_EXCLUDED(mutex_);
  std::unique_ptr<BwuHandler> CreateBwuHandler(
      BwuHandler::IncomingConnectionCallback incoming_connection_callback)
      override;

 protected:
  // Use for unit tests only to inject a WebRtcMedium.
  explicit WebRtcImpl(std::unique_ptr<WebRtcMedium> medium);

  // Used in unit tests to determine how many calls to `AttemptToConnect`
  // occured during a call to `Connect`, per service id.
  std::map<std::string, int> service_id_to_connect_attempts_count_map_;

 private:
  static constexpr int kConnectAttemptsLimit = 3;
  static constexpr int kRestartAcceptConnectionsLimit = 3;

  enum class Role {
    kNone = 0,
    kOfferer = 1,
    kAnswerer = 2,
  };

  struct AcceptingConnectionsInfo {
    // The self_peer_id is generated from the BT/WiFi advertisements and allows
    // the scanner to message us over Tachyon.
    WebrtcPeerId self_peer_id;

    // The registered callback. When there's an incoming connection, this
    // callback is notified.
    AcceptedConnectionCallback accepted_connection_callback;

    // Allows us to communicate with the Tachyon web server.
    std::unique_ptr<WebRtcSignalingMessenger> signaling_messenger;

    // Restarts the tachyon inbox receives messages streaming rpc if the
    // streaming rpc times out. The streaming rpc times out after 60s while
    // advertising. Non-null when listening for WebRTC connections as an
    // offerer.
    std::unique_ptr<CancelableAlarm> restart_tachyon_receive_messages_alarm;

    // Tracks the number of times we've restarted receiving messages after a
    // failure. We limit the number to prevent endless restarts if we are
    // repeatedly unable to communicate with Tachyon.
    int restart_accept_connections_count = 0;
  };

  struct ConnectionRequestInfo {
    // The self_peer_id is randomly generated and allows the advertiser to
    // message us over Tachyon.
    WebrtcPeerId self_peer_id;

    // Allows us to communicate with the Tachyon web server.
    std::unique_ptr<WebRtcSignalingMessenger> signaling_messenger;

    // The pending DataChannel future. Our client will be blocked on this while
    // they wait for us to set up the channel over Tachyon.
    Future<std::shared_ptr<WebRtcSocket>> socket_future;
  };

  // Attempt to initiates a WebRtc connection with peer device identified by
  // |peer_id|.
  // Runs on @MainThread.
  ErrorOr<std::shared_ptr<WebRtcSocket>> AttemptToConnect(
      const std::string& service_id, const WebrtcPeerId& peer_id,
      const location::nearby::connections::LocationHint& location_hint,
      CancellationFlag* cancellation_flag) ABSL_LOCKS_EXCLUDED(mutex_);

  // Returns if the device is accepting connection with specific service id.
  // Runs on @MainThread.
  bool IsAcceptingConnectionsLocked(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Receives a message from the signaling messenger.
  void OnSignalingMessage(const std::string& service_id,
                          const ByteArray& message);

  // Decides whether to restart receiving messages.
  void OnSignalingComplete(const std::string& service_id, bool success);

  // Runs on |single_thread_executor_|.
  void ProcessTachyonInboxMessage(const std::string& service_id,
                                  const ByteArray& message)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Runs on |single_thread_executor_|.
  void SendOffer(const std::string& service_id,
                 const WebrtcPeerId& remote_peer_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  void ReceiveOffer(const WebrtcPeerId& remote_peer_id,
                    SessionDescriptionWrapper offer)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  void SendAnswer(const WebrtcPeerId& remote_peer_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  void ReceiveAnswer(const WebrtcPeerId& remote_peer_id,
                     SessionDescriptionWrapper answer)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  void ReceiveIceCandidates(
      const WebrtcPeerId& remote_peer_id,
      std::vector<std::unique_ptr<webrtc::IceCandidate>> ice_candidates)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  std::unique_ptr<ConnectionFlow> CreateConnectionFlow(
      const std::string& service_id, const WebrtcPeerId& remote_peer_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  std::unique_ptr<ConnectionFlow> GetConnectionFlow(
      const WebrtcPeerId& remote_peer_id) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  void RemoveConnectionFlow(const WebrtcPeerId& remote_peer_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  void ProcessDataChannelOpen(const std::string& service_id,
                              const WebrtcPeerId& remote_peer_id,
                              std::shared_ptr<WebRtcSocket> socket_wrapper)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Runs on |single_thread_executor_|.
  void ProcessDataChannelClosed(const WebrtcPeerId& remote_peer_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Runs on |single_thread_executor_|.
  void ProcessLocalIceCandidate(
      const std::string& service_id, const WebrtcPeerId& remote_peer_id,
      location::nearby::mediums::IceCandidate ice_candidate)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Runs on |single_thread_executor_|.
  void ProcessRestartTachyonReceiveMessages(const std::string& service_id)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Runs on |single_thread_executor_|.
  void RestartTachyonReceiveMessages(const std::string& service_id)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  void AdapterTypeChangedHandler(webrtc::AdapterType adapter_type)
      ABSL_LOCKS_EXCLUDED(mutex_);

  void OffloadFromThread(const std::string& name, Runnable runnable);

  Mutex mutex_;

  std::unique_ptr<WebRtcMedium> medium_;

  // The single thread we throw the potentially blocking work on to.
  ScheduledExecutor single_thread_executor_;

  // A map of ServiceID -> State for all services that are listening for
  // incoming connections.
  absl::flat_hash_map<std::string, AcceptingConnectionsInfo>
      accepting_connections_info_ ABSL_GUARDED_BY(mutex_);

  // A map of a remote PeerId -> State for pending connection requests. As
  // messages from Tachyon come in, this lets us look up the connection request
  // info to handle the interaction.
  absl::flat_hash_map<std::string, ConnectionRequestInfo>
      requesting_connections_info_ ABSL_GUARDED_BY(mutex_);

  // A map of a remote PeerId -> ConnectionFlow. For each connection, we create
  // a unique ConnectionFlow.
  absl::flat_hash_map<std::string, std::unique_ptr<ConnectionFlow>>
      connection_flows_ ABSL_GUARDED_BY(mutex_);

  bool is_using_cellular_ ABSL_GUARDED_BY(mutex_) = true;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_IMPL_H_
