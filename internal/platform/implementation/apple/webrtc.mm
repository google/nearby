// Copyright 2025 Google LLC
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

#include "internal/platform/implementation/apple/webrtc.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "internal/account/account_manager_impl.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/crypto.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/logging.h"
#include "internal/platform/tachyon_express_signaling_messenger.h"
#include "internal/proto/tachyon.pb.h"
#include "internal/proto/tachyon_enums.proto.h"
#include "webrtc/api/create_modular_peer_connection_factory.h"
#include "webrtc/api/task_queue/default_task_queue_factory.h"

namespace nearby::apple {

std::string WebRtcMedium::GetDefaultCountryCode() {
  NSString* countryCode = [NSLocale.currentLocale objectForKey:NSLocaleCountryCode];
  if (countryCode) {
    return std::string([countryCode UTF8String]);
  }
  return "US";
}

void WebRtcMedium::CreatePeerConnection(webrtc::PeerConnectionObserver* observer,
                                        PeerConnectionCallback callback) {
  CreatePeerConnection(std::nullopt, observer, std::move(callback));
}

void WebRtcMedium::CreatePeerConnection(
    std::optional<webrtc::PeerConnectionFactoryInterface::Options> options,
    webrtc::PeerConnectionObserver* observer, PeerConnectionCallback callback) {
  webrtc::PeerConnectionInterface::RTCConfiguration rtc_config;
  rtc_config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
  // TODO: b/261663238 - Add the TURN servers and go beyond the default servers.
  webrtc::PeerConnectionInterface::IceServer ice_server;
  ice_server.urls.emplace_back("stun:stun.l.google.com:19302");
  ice_server.urls.emplace_back("stun:stun1.l.google.com:19302");
  ice_server.urls.emplace_back("stun:stun2.l.google.com:19302");
  ice_server.urls.emplace_back("stun:stun3.l.google.com:19302");
  ice_server.urls.emplace_back("stun:stun4.l.google.com:19302");
  rtc_config.servers.push_back(ice_server);

  std::unique_ptr<webrtc::Thread> signaling_thread = webrtc::Thread::Create();
  signaling_thread->SetName("signaling_thread", nullptr);
  if (!signaling_thread->Start()) {
    callback(/*peer_connection=*/nullptr);
    return;
  }

  webrtc::PeerConnectionDependencies dependencies(observer);
  webrtc::PeerConnectionFactoryDependencies factory_dependencies;
  factory_dependencies.signaling_thread = signaling_thread.release();

  webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory =
      webrtc::CreateModularPeerConnectionFactory(std::move(factory_dependencies));
  if (options.has_value()) {
    peer_connection_factory->SetOptions(options.value());
  }
  webrtc::RTCErrorOr<webrtc::scoped_refptr<webrtc::PeerConnectionInterface>>
      peer_connection_or_error =
          peer_connection_factory->CreatePeerConnectionOrError(rtc_config, std::move(dependencies));
  if (peer_connection_or_error.ok()) {
    callback(peer_connection_or_error.MoveValue());
  } else {
    callback(/*peer_connection=*/nullptr);
  }
}

std::unique_ptr<api::WebRtcSignalingMessenger> WebRtcMedium::GetSignalingMessenger(
    absl::string_view self_id, const location::nearby::connections::LocationHint& location_hint) {
  return std::make_unique<TachyonExpressSignalingMessenger>(self_id, location_hint);
}

}  // namespace nearby::apple
