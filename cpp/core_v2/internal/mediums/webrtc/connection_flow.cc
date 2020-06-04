#include "core_v2/internal/mediums/webrtc/connection_flow.h"

#include <memory>

#include "platform_v2/public/mutex_lock.h"
#include "platform_v2/public/webrtc.h"
#include "absl/memory/memory.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

std::unique_ptr<ConnectionFlow> ConnectionFlow::Create(
    LocalIceCandidateListener local_ice_candidate_listener,
    DataChannelListener data_channel_listener,
    SingleThreadExecutor* single_threaded_executor,
    WebRtcMedium& webrtc_medium) {
  auto connection_flow = absl::WrapUnique(new ConnectionFlow(
      std::move(local_ice_candidate_listener), std::move(data_channel_listener),
      single_threaded_executor));
  if (connection_flow->InitPeerConnection(webrtc_medium)) {
    return connection_flow;
  }

  return nullptr;
}

ConnectionFlow::ConnectionFlow(
    LocalIceCandidateListener local_ice_candidate_listener,
    DataChannelListener data_channel_listener,
    SingleThreadExecutor* single_threaded_executor)
    : data_channel_listener_(std::move(data_channel_listener)),
      peer_connection_observer_(this, std::move(local_ice_candidate_listener),
                                single_threaded_executor) {}

std::unique_ptr<webrtc::SessionDescriptionInterface>
ConnectionFlow::CreateOffer() {
  MutexLock lock(&mutex_);

  // TODO(bfranz): Implement

  return std::unique_ptr<webrtc::SessionDescriptionInterface>();
}

std::unique_ptr<webrtc::SessionDescriptionInterface>
ConnectionFlow::CreateAnswer() {
  MutexLock lock(&mutex_);

  // TODO(bfranz): Implement

  return std::unique_ptr<webrtc::SessionDescriptionInterface>();
}

bool ConnectionFlow::SetLocalSessionDescription(
    std::unique_ptr<webrtc::SessionDescriptionInterface> sdp) {
  MutexLock lock(&mutex_);

  // TODO(bfranz): Implement

  return false;
}

void ConnectionFlow::OnOfferReceived(
    std::unique_ptr<webrtc::SessionDescriptionInterface> offer) {
  MutexLock lock(&mutex_);

  // TODO(bfranz): Implement
}

void ConnectionFlow::OnAnswerReceived(
    std::unique_ptr<webrtc::SessionDescriptionInterface> answer) {
  MutexLock lock(&mutex_);

  // TODO(bfranz): Implement
}

bool ConnectionFlow::OnRemoteIceCandidatesReceived(
    std::vector<webrtc::IceCandidateInterface*> ice_candidates) {
  MutexLock lock(&mutex_);

  // TODO(bfranz): Implement

  return false;
}

api::ListenableFuture<rtc::scoped_refptr<webrtc::DataChannelInterface>>*
ConnectionFlow::GetDataChannel() {
  return static_cast<
      api::ListenableFuture<rtc::scoped_refptr<webrtc::DataChannelInterface>>*>(
      &data_channel_future_);
}

bool ConnectionFlow::Close() {
  MutexLock lock(&mutex_);

  // TODO(bfranz): Implement

  return false;
}

bool ConnectionFlow::InitPeerConnection(WebRtcMedium& webrtc_medium) {
  Future<bool> success_future;
  webrtc_medium.CreatePeerConnection(
      &peer_connection_observer_,
      [this, &success_future](
          rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection) {
        peer_connection_ = peer_connection;
        success_future.Set(true);
      });

  ExceptionOr<bool> result = success_future.Get(kTimeout);
  return result.ok() && result.result();
}

void ConnectionFlow::OnSignalingStable() {
  // TODO(bfranz): Implement
}

void ConnectionFlow::ProcessOnPeerConnectionChange(
    webrtc::PeerConnectionInterface::PeerConnectionState new_state) {
  // TODO(bfranz): Implement
}

webrtc::DataChannelObserver* ConnectionFlow::CreateDataChannelObserver(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  // TODO(bfranz): Implement

  return nullptr;
}
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
