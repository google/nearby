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

#include "platform/impl/g3/webrtc.h"

#include <memory>

#include "platform/base/medium_environment.h"
#include "webrtc/api/task_queue/default_task_queue_factory.h"

namespace location {
namespace nearby {
namespace g3 {

WebRtcSignalingMessenger::WebRtcSignalingMessenger(
    absl::string_view self_id, const connections::LocationHint& location_hint)
    : self_id_(self_id), location_hint_(location_hint) {}

bool WebRtcSignalingMessenger::SendMessage(absl::string_view peer_id,
                                           const ByteArray& message) {
  auto& env = MediumEnvironment::Instance();
  env.SendWebRtcSignalingMessage(peer_id, message);
  return true;
}

bool WebRtcSignalingMessenger::StartReceivingMessages(
    OnSignalingMessageCallback on_message_callback,
    OnSignalingCompleteCallback on_complete_callback) {
  auto& env = MediumEnvironment::Instance();
  env.RegisterWebRtcSignalingMessenger(self_id_, on_message_callback,
                                       on_complete_callback);
  return true;
}

void WebRtcSignalingMessenger::StopReceivingMessages() {
  auto& env = MediumEnvironment::Instance();
  env.UnregisterWebRtcSignalingMessenger(self_id_);
}

WebRtcMedium::~WebRtcMedium() { single_thread_executor_.Shutdown(); }

const std::string WebRtcMedium::GetDefaultCountryCode() { return "US"; }

void WebRtcMedium::CreatePeerConnection(
    webrtc::PeerConnectionObserver* observer, PeerConnectionCallback callback) {
  auto& env = MediumEnvironment::Instance();
  if (!env.GetUseValidPeerConnection()) {
    callback(nullptr);
    return;
  }

  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  webrtc::PeerConnectionDependencies dependencies(observer);

  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  signaling_thread->SetName("signaling_thread", nullptr);
  RTC_CHECK(signaling_thread->Start()) << "Failed to start thread";

  webrtc::PeerConnectionFactoryDependencies factory_dependencies;
  factory_dependencies.task_queue_factory =
      webrtc::CreateDefaultTaskQueueFactory();
  factory_dependencies.signaling_thread = signaling_thread.release();

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection =
      webrtc::CreateModularPeerConnectionFactory(
          std::move(factory_dependencies))
          ->CreatePeerConnection(rtc_config, std::move(dependencies));

  single_thread_executor_.Execute(
      [&env, callback = std::move(callback),
       peer_connection = std::move(peer_connection)]() {
        absl::SleepFor(env.GetPeerConnectionLatency());
        callback(peer_connection);
      });
}

std::unique_ptr<api::WebRtcSignalingMessenger>
WebRtcMedium::GetSignalingMessenger(
    absl::string_view self_id, const connections::LocationHint& location_hint) {
  return std::make_unique<WebRtcSignalingMessenger>(self_id, location_hint);
}

}  // namespace g3
}  // namespace nearby
}  // namespace location
