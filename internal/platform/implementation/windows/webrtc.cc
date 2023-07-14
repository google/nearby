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
#include <optional>
#include <string>
#include <utility>

#include "internal/account/account_manager_impl.h"
#include "internal/platform/logging.h"
#include "internal/platform/tachyon_messaging_client.h"
#include "internal/proto/tachyon.pb.h"
#include "webrtc/api/task_queue/default_task_queue_factory.h"

namespace nearby {
namespace windows {

WebRtcSignalingMessenger::WebRtcSignalingMessenger(
    absl::string_view self_id,
    const location::nearby::connections::LocationHint& location_hint)
    : self_id_(self_id),
      location_hint_(location_hint),
      account_manager_(nearby::AccountManagerImpl::Factory::instance()),
      client_(new TachyonMessagingClient()) {}

bool WebRtcSignalingMessenger::SendMessage(absl::string_view peer_id,
                                           const ByteArray& message) {
  auto request =
      std::make_unique<google::internal::communications::instantmessaging::v1::
                           SendMessageExpressRequest>();
  google::internal::communications::instantmessaging::v1::Id id = google::
      internal::communications::instantmessaging::v1::Id::default_instance();
  id.set_id(self_id_);
  id.set_app(kApp);
  id.set_type(google::internal::communications::instantmessaging::v1::IdType::
                  NEARBY_ID);
  id.mutable_location_hint()->set_location(location_hint_.location());
  id.mutable_location_hint()->set_format(
      google::internal::communications::instantmessaging::v1::LocationStandard::
          ISO_3166_1_ALPHA_2);

  request->mutable_dest_id()->set_id(peer_id);
  request->mutable_dest_id()->set_app(kApp);
  request->mutable_dest_id()->set_type(
      google::internal::communications::instantmessaging::v1::IdType::
          NEARBY_ID);
  request->mutable_header()->mutable_requester_id()->set_id(self_id_);
  request->mutable_header()->mutable_requester_id()->set_app(kApp);
  request->mutable_header()->mutable_requester_id()->set_type(
      google::internal::communications::instantmessaging::v1::IdType::
          NEARBY_ID);
  request->mutable_message()->set_message(message.string_data());

  // Generate a random message identifier.
  std::string bytes(16, 0);
  crypto::RandBytes(const_cast<std::string::value_type*>(bytes.data()),
                    bytes.size());
  request->mutable_message()->set_message_id(bytes);
  request->mutable_message()->set_message_type(
      google::internal::communications::instantmessaging::v1::InboxMessage::
          BASIC);
  request->mutable_message()->set_message_class(
      google::internal::communications::instantmessaging::v1::InboxMessage::
          EPHEMERAL);
  request->mutable_header()->set_app(kApp);

  std::optional<AccountManager::Account> account =
      account_manager_->GetCurrentAccount();
  if (!account.has_value()) {
    NEARBY_LOGS(INFO) << "Failed to get current account.";
    return false;
  }
  account_manager_->GetAccessToken(
      account->id,
      /*success_callback=*/
      [&](absl::string_view token) {
        NEARBY_LOGS(INFO) << "Access token returned";
        absl::Status status = client_->SendMessageExpress(
            request.get(), location_hint_.location(), std::string{token});
      },
      /*failure_callback=*/
      [=](absl::Status status) {
        NEARBY_LOGS(INFO) << "Failed to receive access token.";
      });

  return true;
}

bool WebRtcSignalingMessenger::StartReceivingMessages(
    api::WebRtcSignalingMessenger::OnSignalingMessageCallback
        on_message_callback,
    api::WebRtcSignalingMessenger::OnSignalingCompleteCallback
        on_complete_callback) {
  std::optional<AccountManager::Account> account =
      account_manager_->GetCurrentAccount();
  if (!account.has_value()) {
    NEARBY_LOGS(INFO) << "Failed to get current account.";
    return false;
  }
  NEARBY_LOGS(INFO) << __func__ << "Access token?";
  account_manager_->GetAccessToken(
      account->id,
      /*success_callback=*/
      [&](absl::string_view token) {
        NEARBY_LOGS(INFO) << "Access token returned";
        NEARBY_LOGS(INFO) << "Attempting to receive message";
        absl::Status status = client_->ReceiveMessagesExpress(
            self_id_, location_hint_.location(), std::move(on_message_callback),
            std::move(on_complete_callback), std::string{token});
        NEARBY_LOGS(INFO) << "WebRTC: Exiting access token callback";
      },
      /*failure_callback=*/
      [=](absl::Status status) {
        NEARBY_LOGS(INFO) << "Failed to receive access token.";
      });
  NEARBY_LOGS(INFO) << __func__ << "Goodbye";

  return true;
}
void WebRtcSignalingMessenger::StopReceivingMessages() {
  client_->StopReceivingMessages();
}

const std::string WebRtcMedium::GetDefaultCountryCode() {
  wchar_t systemGeoName[LOCALE_NAME_MAX_LENGTH];

  if (!GetUserDefaultGeoName(systemGeoName, LOCALE_NAME_MAX_LENGTH)) {
    NEARBY_LOGS(ERROR) << __func__ << ": Failed to GetUserDefaultGeoName: "
                       << ". Fall back to US.";
    return "US";
  }
  std::wstring wideGeo(systemGeoName);
  std::string systemGeoNameString(wideGeo.begin(), wideGeo.end());
  NEARBY_LOGS(VERBOSE) << "GetUserDefaultGeoName() returns: "
                       << systemGeoNameString;
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
    NEARBY_LOGS(FATAL) << "Failed to start thread";
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
    NEARBY_LOGS(FATAL) << "Failed to create peer connection";
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
