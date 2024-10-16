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

#include "internal/platform/implementation/g3/webrtc.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/webrtc.h"
#include "internal/platform/medium_environment.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/task_queue/default_task_queue_factory.h"
#include "webrtc/rtc_base/checks.h"

namespace nearby {
namespace g3 {
using ::location::nearby::connections::LocationHint;

WebRtcSignalingMessenger::WebRtcSignalingMessenger(
    absl::string_view self_id, const LocationHint& location_hint)
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
  env.RegisterWebRtcSignalingMessenger(self_id_, std::move(on_message_callback),
                                       std::move(on_complete_callback));
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
  CreatePeerConnection(std::nullopt, observer, std::move(callback));
}

void WebRtcMedium::CreatePeerConnection(
    std::optional<webrtc::PeerConnectionFactoryInterface::Options> options,
    webrtc::PeerConnectionObserver* observer, PeerConnectionCallback callback) {
  auto& env = MediumEnvironment::Instance();
  if (!env.GetUseValidPeerConnection()) {
    callback(nullptr);
    return;
  }

  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  webrtc::PeerConnectionDependencies dependencies(observer);

  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  signaling_thread->SetName("signaling_thread", nullptr);
  RTC_CHECK(signaling_thread->Start()) << "Failed to start thread";

  webrtc::PeerConnectionFactoryDependencies factory_dependencies;
  factory_dependencies.task_queue_factory =
      webrtc::CreateDefaultTaskQueueFactory();
  factory_dependencies.signaling_thread = signaling_thread.release();

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory = webrtc::CreateModularPeerConnectionFactory(
          std::move(factory_dependencies));
  RTC_CHECK(peer_connection_factory != nullptr)
      << "Failed to create peer connection factory";
  if (options.has_value()) {
    peer_connection_factory->SetOptions(options.value());
  }
  auto peer_connection_or_error =
      peer_connection_factory->CreatePeerConnectionOrError(
          rtc_config, std::move(dependencies));
  RTC_CHECK(peer_connection_or_error.ok())
      << "Failed to create peer connection";

  single_thread_executor_.Execute(
      [&env, callback = std::move(callback),
       peer_connection = peer_connection_or_error.MoveValue()]() mutable {
        absl::SleepFor(env.GetPeerConnectionLatency());
        callback(peer_connection);
      });
}

std::unique_ptr<api::WebRtcSignalingMessenger>
WebRtcMedium::GetSignalingMessenger(absl::string_view self_id,
                                    const LocationHint& location_hint) {
  return std::make_unique<WebRtcSignalingMessenger>(self_id, location_hint);
}

}  // namespace g3
}  // namespace nearby
