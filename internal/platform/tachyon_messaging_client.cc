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

#include "internal/platform/tachyon_messaging_client.h"

#include <ctime>
#include <memory>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "third_party/grpc/include/grpc/support/time.h"
#include "third_party/grpc/include/grpcpp/channel.h"
#include "third_party/grpc/include/grpcpp/client_context.h"
#include "third_party/grpc/include/grpcpp/create_channel.h"
#include "third_party/grpc/include/grpcpp/security/credentials.h"
#include "third_party/grpc/include/grpcpp/support/client_callback.h"
#include "third_party/grpc/include/grpcpp/support/status.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/for_connecting_to_google_roots_pem.h"
#include "internal/platform/implementation/webrtc.h"
#include "internal/platform/logging.h"
#include "internal/proto/messaging.grpc.pb.h"
#include "internal/proto/tachyon.proto.h"
#include "internal/proto/tachyon_common.proto.h"
#include "internal/proto/tachyon_enums.proto.h"
#include "internal/rpc/utils.h"
#include "util/random/mt_random.h"
#include "util/random/util.h"

namespace nearby {

namespace {
using ::google::internal::communications::instantmessaging::v1::ClientInfo;
using ::google::internal::communications::instantmessaging::v1::
    ReceiveMessagesResponse;
using ::google::internal::communications::instantmessaging::v1::RequestHeader;
using ::google::internal::communications::instantmessaging::v1::
    SendMessageExpressRequest;
using ::google::internal::communications::instantmessaging::v1::
    SendMessageExpressResponse;

constexpr absl::string_view kApp = "Nearby";
constexpr absl::string_view kTachyonAddress =
    "instantmessaging-pa.googleapis.com:443";

void InitRequestHeader(RequestHeader* header) {
  // NOTE: It is the caller's responsibility to properly populate the self id
  // and destination id of the header.
  ClientInfo* client_info = header->mutable_client_info();
  client_info->set_platform_type(google::internal::communications::
                                     instantmessaging::v1::Platform::DESKTOP);
  // It is unclear to me where these magic numbers are from but they are used
  // across both the Android and CrOS impls.
  // See:
  // https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/nearby_sharing/tachyon_ice_config_fetcher.cc;l=53
  client_info->set_major(1);
  client_info->set_minor(24);
  client_info->set_point(0);

  client_info->set_api_version(
      google::internal::communications::instantmessaging::v1::ApiVersion::V4);

  // Generate a random message identifier.
  MTRandom rand;
  header->set_request_id(util_random::RandomString(&rand, /*length=*/13,
                                                   util::random::kWebsafe64));
  header->set_app(kApp);
}

}  // namespace

TachyonMessagingClient::ReceiveMessagesReader::ReceiveMessagesReader(
    google::internal::communications::instantmessaging::v1::grpc::Messaging::
        StubInterface* stub,
    absl::string_view self_id, absl::string_view access_token,
    absl::string_view location_hint,
    api::WebRtcSignalingMessenger::OnSignalingMessageCallback
        on_message_callback,
    api::WebRtcSignalingMessenger::OnSignalingCompleteCallback
        on_complete_callback)
    : on_message_callback_(std::move(on_message_callback)),
      on_complete_callback_(std::move(on_complete_callback)) {
  const std::shared_ptr<grpc::CallCredentials> call_creds =
      grpc::AccessTokenCredentials(std::string(access_token));
  context_.set_credentials(call_creds);
  gpr_timespec deadline = gpr_now(GPR_CLOCK_MONOTONIC);
  timespec timespec = absl::ToTimespec(absl::Seconds(30));
  deadline.tv_sec += timespec.tv_sec;
  deadline.tv_nsec += timespec.tv_nsec;
  context_.set_deadline(deadline);

  InitRequestHeader(request_.mutable_header());
  auto* requester_id = request_.mutable_header()->mutable_requester_id();
  requester_id->set_id(self_id);
  requester_id->set_type(google::internal::communications::instantmessaging::
                             v1::IdType::NEARBY_ID);
  requester_id->set_app(kApp);
  auto* request_location_hint = requester_id->mutable_location_hint();
  request_location_hint->set_location(location_hint);
  request_location_hint->set_format(
      google::internal::communications::instantmessaging::v1::LocationStandard::
          ISO_3166_1_ALPHA_2);

  stub->async()->ReceiveMessagesExpress(&context_, &request_, this);
  StartRead(&response_);
  StartCall();
}

void TachyonMessagingClient::ReceiveMessagesReader::OnReadDone(bool ok) {
  if (ok) {
    switch (response_.body_case()) {
      case ReceiveMessagesResponse::kInboxMessage:
        on_message_callback_(ByteArray(response_.inbox_message().message()));
        break;
      default:
        break;
    }
    StartRead(&response_);
  }
}

void TachyonMessagingClient::ReceiveMessagesReader::OnDone(
    const grpc::Status& s) {
  if (!s.ok()) {
    LOG(ERROR) << "ReceiveMessagesExpress failed: "
               << rpc::GrpcStatusToAbslStatus(s);
  }
  on_complete_callback_(s.ok());
}

void TachyonMessagingClient::ReceiveMessagesReader::TryCancel() {
  context_.TryCancel();
}

TachyonMessagingClient::TachyonMessagingClient() {
  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(
      std::string(kTachyonAddress), CreateChannelCredentials());
  messaging_stub_ = google::internal::communications::instantmessaging::v1::
      grpc::Messaging::NewStub(channel);
}

TachyonMessagingClient::~TachyonMessagingClient() { StopReceivingMessages(); }

void TachyonMessagingClient::ReceiveMessagesExpress(
    absl::string_view self_id, absl::string_view access_token,
    absl::string_view location_hint,
    api::WebRtcSignalingMessenger::OnSignalingMessageCallback
        on_message_callback,
    api::WebRtcSignalingMessenger::OnSignalingCompleteCallback
        on_complete_callback) {
  reader_ = std::make_unique<ReceiveMessagesReader>(
      messaging_stub_.get(), self_id, access_token, location_hint,
      std::move(on_message_callback), std::move(on_complete_callback));
}

void TachyonMessagingClient::StopReceivingMessages() { reader_->TryCancel(); }

struct SendMessageExpressState {
  grpc::ClientContext context;
  SendMessageExpressRequest request;
  SendMessageExpressResponse response;
  absl::AnyInvocable<void(const absl::Status& status) &&> callback;
};

void TachyonMessagingClient::SendMessageExpress(
    absl::string_view self_id, absl::string_view peer_id,
    const ByteArray& message, absl::string_view access_token,
    absl::string_view location_hint,
    absl::AnyInvocable<void(absl::Status response)> callback) {
  auto rpc_state = std::make_shared<SendMessageExpressState>();

  rpc_state->callback = std::move(callback);

  InitRequestHeader(rpc_state->request.mutable_header());
  rpc_state->request.mutable_header()->set_app(kApp);

  auto* dest_id = rpc_state->request.mutable_dest_id();
  dest_id->set_id(peer_id);
  dest_id->set_app(kApp);
  dest_id->set_type(google::internal::communications::instantmessaging::v1::
                        IdType::NEARBY_ID);
  auto* request_location_hint = dest_id->mutable_location_hint();
  request_location_hint->set_location(location_hint);
  request_location_hint->set_format(
      google::internal::communications::instantmessaging::v1::LocationStandard::
          ISO_3166_1_ALPHA_2);

  auto* requester_id =
      rpc_state->request.mutable_header()->mutable_requester_id();
  requester_id->set_id(self_id);
  requester_id->set_app(kApp);
  requester_id->set_type(google::internal::communications::instantmessaging::
                             v1::IdType::NEARBY_ID);

  auto* request_message = rpc_state->request.mutable_message();
  request_message->set_message(message.string_data());
  request_message->set_message_type(
      google::internal::communications::instantmessaging::v1::InboxMessage::
          BASIC);
  request_message->set_message_class(
      google::internal::communications::instantmessaging::v1::InboxMessage::
          EPHEMERAL);
  MTRandom rand;
  request_message->set_message_id(util_random::RandomString(
      &rand, /*length=*/13, util::random::kWebsafe64));

  const std::shared_ptr<grpc::CallCredentials> call_creds =
      grpc::AccessTokenCredentials(std::string(access_token));
  rpc_state->context.set_credentials(call_creds);
  gpr_timespec deadline = gpr_now(GPR_CLOCK_MONOTONIC);
  timespec timespec = absl::ToTimespec(absl::Seconds(30));
  deadline.tv_sec += timespec.tv_sec;
  deadline.tv_nsec += timespec.tv_nsec;
  rpc_state->context.set_deadline(deadline);

  messaging_stub_->async()->SendMessageExpress(
      &rpc_state->context, &rpc_state->request, &rpc_state->response,
      [rpc_state](grpc::Status rpc_status) mutable {
        if (!rpc_status.ok()) {
          absl::Status status = rpc::GrpcStatusToAbslStatus(rpc_status);
          std::move(rpc_state->callback)(status);
          return;
        }
        std::move(rpc_state->callback)(absl::OkStatus());
      });
}

// Creates the gRPC channel credentials configured to use SSL and to add the
// API key for each call.
// If we need google.com, we need additional headers. See: go/grpc-google-com.
std::shared_ptr<grpc::ChannelCredentials>
TachyonMessagingClient::CreateChannelCredentials() {
  auto toc = for_connecting_to_google_roots_pem_create();
  std::string root_certificates =
      std::string(absl::string_view(toc[0].data, toc[0].size));
  grpc::SslCredentialsOptions cred_options;
  cred_options.pem_root_certs = root_certificates;
  std::shared_ptr<grpc::ChannelCredentials> channel_creds =
      grpc::SslCredentials(cred_options);
  return channel_creds;
}

}  // namespace nearby
