// Copyright 2023 Google LLC
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

#include "internal/platform/implementation/windows/webrtc.h"

#include <winnls.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "internal/account/account_manager_impl.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/webrtc.h"
#include "internal/platform/logging.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/task_queue/default_task_queue_factory.h"
#include "webrtc/rtc_base/thread.h"

namespace nearby {
namespace windows {

WebRtcSignalingMessenger::WebRtcSignalingMessenger(
    absl::string_view self_id,
    const location::nearby::connections::LocationHint& location_hint)
    : self_id_(self_id),
      location_hint_(location_hint),
      account_manager_(nearby::AccountManagerImpl::Factory::instance()) {}
// TODO(b/261663238): replace with real implementation.
bool WebRtcSignalingMessenger::SendMessage(absl::string_view peer_id,
                                           const ByteArray& message) {
  return false;
}

// TODO(b/261663238): replace with real implementation.
bool WebRtcSignalingMessenger::StartReceivingMessages(
    api::WebRtcSignalingMessenger::OnSignalingMessageCallback
        on_message_callback,
    api::WebRtcSignalingMessenger::OnSignalingCompleteCallback
        on_complete_callback) {
  return false;
}
// TODO(b/261663238): replace with real implementation.
void WebRtcSignalingMessenger::StopReceivingMessages() {}

const std::string WebRtcMedium::GetDefaultCountryCode() {
  wchar_t systemGeoName[LOCALE_NAME_MAX_LENGTH];

  if (!GetUserDefaultGeoName(systemGeoName, LOCALE_NAME_MAX_LENGTH)) {
    LOG(ERROR) << __func__
               << ": Failed to GetUserDefaultGeoName: " << ". Fall back to US.";
    return "US";
  }
  std::wstring wideGeo(systemGeoName);
  std::string systemGeoNameString(wideGeo.begin(), wideGeo.end());
  VLOG(1) << "GetUserDefaultGeoName() returns: " << systemGeoNameString;
  return systemGeoNameString;
}

void WebRtcMedium::CreatePeerConnection(
    webrtc::PeerConnectionObserver* observer, PeerConnectionCallback callback) {
  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  // TODO(b/261663238): Add the TURN servers and go beyond the default servers.
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.urls.emplace_back("stun:stun.l.google.com:19302");
  ice_server.urls.emplace_back("stun:stun1.l.google.com:19302");
  ice_server.urls.emplace_back("stun:stun2.l.google.com:19302");
  ice_server.urls.emplace_back("stun:stun3.l.google.com:19302");
  ice_server.urls.emplace_back("stun:stun4.l.google.com:19302");
  rtc_config.servers.push_back(ice_server);

  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  signaling_thread->SetName("signaling_thread", nullptr);
  if (!signaling_thread->Start()) {
    LOG(FATAL) << "Failed to start thread";
  }

  webrtc::PeerConnectionDependencies dependencies(observer);
  webrtc::PeerConnectionFactoryDependencies factory_dependencies;
  factory_dependencies.task_queue_factory =
      webrtc::CreateDefaultTaskQueueFactory();
  factory_dependencies.signaling_thread = signaling_thread.release();

  auto peer_connection_or_error =
      webrtc::CreateModularPeerConnectionFactory(
          std::move(factory_dependencies))
          ->CreatePeerConnectionOrError(rtc_config, std::move(dependencies));

  if (peer_connection_or_error.ok()) {
    callback(peer_connection_or_error.MoveValue());
  } else {
    LOG(FATAL) << "Failed to create peer connection";
    callback(/*peer_connection=*/nullptr);
  }
}

std::unique_ptr<api::WebRtcSignalingMessenger>
WebRtcMedium::GetSignalingMessenger(
    absl::string_view self_id,
    const location::nearby::connections::LocationHint& location_hint) {
  return std::make_unique<WebRtcSignalingMessenger>(std::string(self_id),
                                                    location_hint);
}

}  // namespace windows
}  // namespace nearby
