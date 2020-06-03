#ifndef CORE_V2_INTERNAL_MEDIUMS_WEBRTC_CONNECTION_FLOW_H_
#define CORE_V2_INTERNAL_MEDIUMS_WEBRTC_CONNECTION_FLOW_H_

#include <memory>

#include "core_v2/internal/mediums/webrtc/data_channel_listener.h"
#include "core_v2/internal/mediums/webrtc/local_ice_candidate_listener.h"
#include "core_v2/internal/mediums/webrtc/peer_connection_observer_impl.h"
#include "platform_v2/base/runnable.h"
#include "platform_v2/public/future.h"
#include "platform_v2/public/single_thread_executor.h"
#include "platform_v2/public/webrtc.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/peer_connection_interface.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

/**
 * Flow for an offerer:
 *
 * <ul>
 *   <li>INITIALIZED: After construction.
 *   <li>CREATING_OFFER: After CreateOffer(). Local ice candidate collection
 * begins.
 *   <li>WAITING_FOR_ANSWER: Until the remote peer sends their answer.
 *   <li>WAITING_TO_CONNECT: Until the data channel actually connects. Remote
 * ice candidates should be added with OnRemoteIceCandidatesReceived as they are
 * gathered.
 *   <li>CONNECTED: We successfully connected to the remote data
 * channel.
 *   <li>ENDED: The final state that can occur from any of the previous
 * states if we disconnect at any point in the flow.
 * </ul>
 *
 * <p>Flow for an answerer:
 *
 * <ul>
 *   <li>INITIALIZED: After construction.
 *   <li>RECEIVED_OFFER: After onOfferReceived().
 *   <li>CREATING_ANSWER: After CreateAnswer(). Local ice candidate collection
 * begins.
 *   <li>WAITING_TO_CONNECT: Until the data channel actually connects.
 * Remote ice candidates should be added with OnRemoteIceCandidatesReceived as
 * they are gathered.
 *   <li>CONNECTED: We successfully connected to the remote
 * data channel.
 *   <li>ENDED: The final state that can occur from any of the
 * previous states if we disconnect at any point in the flow.
 * </ul>
 */
class ConnectionFlow {
 public:
  // This method blocks on the creation of the peer connection object.
  static std::unique_ptr<ConnectionFlow> Create(
      LocalIceCandidateListener local_ice_candidate_listener,
      DataChannelListener data_channel_listener,
      SingleThreadExecutor* single_threaded_executor,
      WebRtcMedium& webrtc_medium);
  ~ConnectionFlow() = default;

  // Create the offer that will be sent to the remote. Mirrors the behaviour of
  // PeerConnectionInterface::CreateOffer.
  std::unique_ptr<webrtc::SessionDescriptionInterface> CreateOffer()
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Create the answer that will be sent to the remote. Mirrors the behaviour of
  // PeerConnectionInterface::CreateAnswer.
  std::unique_ptr<webrtc::SessionDescriptionInterface> CreateAnswer()
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Set the local session description. |sdp| was created via CreateOffer()
  // or CreateAnswer().
  bool SetLocalSessionDescription(
      std::unique_ptr<webrtc::SessionDescriptionInterface> sdp)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Invoked when an offer was received from a remote; this will set the remote
  // session description on the peer connection.
  void OnOfferReceived(
      std::unique_ptr<webrtc::SessionDescriptionInterface> offer)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Invoked when an answer was received from a remote; this will set the remote
  // session description on the peer connection.
  void OnAnswerReceived(
      std::unique_ptr<webrtc::SessionDescriptionInterface> answer)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Invoked when an ice candidate was received from a remote; this will add the
  // ice candidate to the peer connection if ready or cache it otherwise.
  bool OnRemoteIceCandidatesReceived(
      std::vector<webrtc::IceCandidateInterface*> ice_candidates)
      ABSL_LOCKS_EXCLUDED(mutex_);
  // Get a future for the data channel.
  api::ListenableFuture<rtc::scoped_refptr<webrtc::DataChannelInterface>>*
  GetDataChannel();
  // Close the peer connection and data channel.
  bool Close() ABSL_LOCKS_EXCLUDED(mutex_);

  // Invoked when the peer connection indicates that signaling is stable.
  void OnSignalingStable();
  webrtc::DataChannelObserver* CreateDataChannelObserver(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);

  // Invoked upon changes in the state of peer connection, e.g. react to
  // disconnect.
  void ProcessOnPeerConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state);

 private:
  ConnectionFlow(LocalIceCandidateListener local_ice_candidate_listener,
                 DataChannelListener data_channel_listener,
                 SingleThreadExecutor* single_threaded_executor);

  // TODO(bfranz): Consider whether this needs to be configurable per platform
  static constexpr absl::Duration kTimeout = absl::Milliseconds(250);

  bool InitPeerConnection(WebRtcMedium& webrtc_medium);

  DataChannelListener data_channel_listener_;

  Future<rtc::scoped_refptr<webrtc::DataChannelInterface>> data_channel_future_;

  PeerConnectionObserverImpl peer_connection_observer_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;

  Mutex mutex_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_WEBRTC_CONNECTION_FLOW_H_
