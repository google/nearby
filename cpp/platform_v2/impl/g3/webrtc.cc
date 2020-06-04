#include "platform_v2/impl/g3/webrtc.h"

#include "webrtc/files/stable/webrtc/api/task_queue/default_task_queue_factory.h"

namespace location {
namespace nearby {
namespace g3 {

void WebRtcMedium::CreatePeerConnection(
    webrtc::PeerConnectionObserver* observer, PeerConnectionCallback callback) {
  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  webrtc::PeerConnectionDependencies dependencies(observer);

  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  signaling_thread->SetName("signaling_thread", nullptr);
  RTC_CHECK(signaling_thread->Start()) << "Failed to start thread";

  webrtc::PeerConnectionFactoryDependencies factory_dependencies;
  factory_dependencies.task_queue_factory =
      webrtc::CreateDefaultTaskQueueFactory();
  factory_dependencies.signaling_thread = signaling_thread.release();

  callback(webrtc::CreateModularPeerConnectionFactory(
               std::move(factory_dependencies))
               ->CreatePeerConnection(rtc_config, std::move(dependencies)));
}

std::unique_ptr<api::WebRtcSignalingMessenger>
WebRtcMedium::GetSignalingMessenger(absl::string_view self_id) {
  // TODO(bfranz): Implement
  return nullptr;
}

}  // namespace g3
}  // namespace nearby
}  // namespace location
