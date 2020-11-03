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

  // Returns if the device is ready to accept connections from remote devices.
  // Runs on @MainThread.
  bool IsAcceptingConnections() ABSL_LOCKS_EXCLUDED(mutex_);

  // Prepares the device to accept incoming WebRtc connections. Returns a
  // boolean value indicating if the device has started accepting connections.
  // Runs on @MainThread.
  bool StartAcceptingConnections(const PeerId& self_id,
                                 const LocationHint& location_hint,
                                 AcceptedConnectionCallback callback)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Prevents device from accepting future connections until
  // StartAcceptingConnections() is called.
  // Runs on @MainThread.
  void StopAcceptingConnections() ABSL_LOCKS_EXCLUDED(mutex_);

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

  bool InitWebRtcFlow(Role role, const PeerId& self_id,
                      const LocationHint& location_hint)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  Future<WebRtcSocketWrapper> ListenForWebRtcSocketFuture(
      Future<rtc::scoped_refptr<webrtc::DataChannelInterface>>
          data_channel_future,
      AcceptedConnectionCallback callback);

  WebRtcSocketWrapper CreateWebRtcSocketWrapper(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

  LocalIceCandidateListener GetLocalIceCandidateListener();
  void OnLocalIceCandidate(
      const webrtc::IceCandidateInterface* local_ice_candidate);

  DataChannelListener GetDataChannelListener();
  void OnDataChannelClosed();
  void OnDataChannelMessageReceived(const ByteArray& message);
  void OnDataChannelBufferedAmountChanged();

  // Runs on @MainThread and |single_thread_executor_|.
  bool SetLocalSessionDescription(SessionDescriptionWrapper sdp)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  bool IsSignaling() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  void ProcessSignalingMessage(const ByteArray& message)
      ABSL_LOCKS_EXCLUDED(mutex_);

  // Runs on |single_thread_executor_|.
  void SendOfferAndIceCandidatesToPeer() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on |single_thread_executor_|.
  void SendAnswerToPeer() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on @MainThread and |single_thread_executor_|.
  void LogAndDisconnect(const std::string& error_message)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on @MainThread.
  void Disconnect() ABSL_LOCKS_EXCLUDED(mutex_);

  // Runs on @MainThread and |single_thread_executor_|.
  void DisconnectLocked() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  void LogAndShutdownSignaling(const std::string& error_message)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on @MainThread and |single_thread_executor_|.
  void ShutdownSignaling() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on @MainThread and |single_thread_executor_|.
  void ShutdownWebRtcSocket() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Runs on @MainThread and |single_thread_executor_|.
  void ShutdownIceCandidateCollection();

  void OffloadFromSignalingThread(Runnable runnable);

  // Runs on |restart_receive_messages_executor_|.
  void RestartReceiveMessages(const LocationHint& location_hint)
      ABSL_LOCKS_EXCLUDED(mutex_);

  Mutex mutex_;

  Role role_ ABSL_GUARDED_BY(mutex_) = Role::kNone;
  PeerId self_id_ ABSL_GUARDED_BY(mutex_);
  PeerId peer_id_ ABSL_GUARDED_BY(mutex_);
  ByteArray pending_local_offer_ ABSL_GUARDED_BY(mutex_);
  std::vector<::location::nearby::mediums::IceCandidate>
      pending_local_ice_candidates_ ABSL_GUARDED_BY(mutex_);

  WebRtcMedium medium_;
  std::unique_ptr<ConnectionFlow> connection_flow_;
  std::unique_ptr<WebRtcSignalingMessenger> signaling_messenger_
      ABSL_GUARDED_BY(mutex_);
  WebRtcSocketWrapper socket_ ABSL_GUARDED_BY(mutex_);

  SingleThreadExecutor single_thread_executor_;

  // Restarts the signaling messenger for receiving messages.
  ScheduledExecutor restart_receive_messages_executor_;
  CancelableAlarm restart_receive_messages_alarm_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_H_
