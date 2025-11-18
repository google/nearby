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

#include "internal/platform/tachyon_express_signaling_messenger.h"

#include <ctime>
#include <memory>
#include <string>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "third_party/grpc/include/grpc/support/time.h"
#include "third_party/grpc/include/grpcpp/channel.h"
#include "third_party/grpc/include/grpcpp/client_context.h"
#include "third_party/grpc/include/grpcpp/create_channel.h"
#include "third_party/grpc/include/grpcpp/security/credentials.h"
#include "third_party/grpc/include/grpcpp/support/client_callback.h"
#include "third_party/grpc/include/grpcpp/support/status.h"
#include "internal/account/account_manager_impl.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/count_down_latch.h"
#include "internal/platform/implementation/account_manager.h"
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
using ::google::internal::communications::instantmessaging::v1::Id;
using ::google::internal::communications::instantmessaging::v1::
    LocationStandard;
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

// It is unclear to me where these magic numbers are from but they are used
// across both the Android and CrOS implementations.
// See:
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/nearby_sharing/tachyon_ice_config_fetcher.cc;l=53
constexpr int kMajorVersion = 1;
constexpr int kMinorVersion = 24;
constexpr int kPointVersion = 0;

void InitId(Id& id, absl::string_view id_str,
            const location::nearby::connections::LocationHint& location_hint) {
  id.set_id(id_str);
  id.set_app(kApp);
  id.set_type(google::internal::communications::instantmessaging::v1::IdType::
                  NEARBY_ID);
  auto* request_location_hint = id.mutable_location_hint();
  request_location_hint->set_location(location_hint.location());
  if (location_hint.format() ==
      location::nearby::connections::LocationStandard::E164_CALLING) {
    request_location_hint->set_format(LocationStandard::E164_CALLING);
  } else if (location_hint.format() ==
             location::nearby::connections::LocationStandard::
                 ISO_3166_1_ALPHA_2) {
    request_location_hint->set_format(LocationStandard::ISO_3166_1_ALPHA_2);
  } else {
    request_location_hint->set_format(LocationStandard::UNKNOWN);
  }
}

void InitRequestHeader(
    RequestHeader& header, absl::string_view self_id,
    const location::nearby::connections::LocationHint& location_hint) {
  ClientInfo* client_info = header.mutable_client_info();
  client_info->set_platform_type(google::internal::communications::
                                     instantmessaging::v1::Platform::DESKTOP);

  client_info->set_major(kMajorVersion);
  client_info->set_minor(kMinorVersion);
  client_info->set_point(kPointVersion);

  client_info->set_api_version(
      google::internal::communications::instantmessaging::v1::ApiVersion::V4);

  // Generate a random message identifier.
  MTRandom rand;
  header.set_request_id(util_random::RandomString(&rand, /*length=*/13,
                                                  util::random::kWebsafe64));
  header.set_app(kApp);

  InitId(*header.mutable_requester_id(), self_id, location_hint);
}

}  // namespace

bool TachyonExpressSignalingMessenger::ReceiveMessagesReader::Start(
    google::internal::communications::instantmessaging::v1::grpc::Messaging::
        StubInterface* stub,
    absl::string_view self_id,
    const location::nearby::connections::LocationHint& location_hint,
    absl::string_view access_token,
    absl::AnyInvocable<void()> on_fast_path_ready_callback,
    absl::AnyInvocable<void(const ByteArray&)> on_inbox_message_callback,
    absl::AnyInvocable<void(bool)> on_complete_callback) {
  {
    absl::MutexLock lock(mutex_);
    if (is_receiving_messages_) {
      return false;
    }
    is_receiving_messages_ = true;
  }

  on_fast_path_ready_callback_ = std::move(on_fast_path_ready_callback);
  on_inbox_message_callback_ = std::move(on_inbox_message_callback);
  on_complete_callback_ = std::move(on_complete_callback);
  const std::shared_ptr<grpc::CallCredentials> call_creds =
      grpc::AccessTokenCredentials(std::string(access_token));
  context_.set_credentials(call_creds);
  gpr_timespec deadline = gpr_now(GPR_CLOCK_MONOTONIC);
  timespec timespec = absl::ToTimespec(absl::Seconds(30));
  deadline.tv_sec += timespec.tv_sec;
  deadline.tv_nsec += timespec.tv_nsec;
  context_.set_deadline(deadline);

  InitRequestHeader(*request_.mutable_header(), self_id, location_hint);
  stub->async()->ReceiveMessagesExpress(&context_, &request_, this);
  StartRead(&response_);
  StartCall();
  return true;
}

void TachyonExpressSignalingMessenger::ReceiveMessagesReader::OnReadDone(
    bool ok) {
  {
    absl::MutexLock lock(mutex_);
    if (!is_receiving_messages_) {
      return;
    }
  }
  if (ok) {
    switch (response_.body_case()) {
      case ReceiveMessagesResponse::kFastPathReady:
        on_fast_path_ready_callback_();
        break;
      case ReceiveMessagesResponse::kInboxMessage:
        on_inbox_message_callback_(
            ByteArray(response_.inbox_message().message()));
        break;
      default:
        break;
    }
    StartRead(&response_);
  }
}

void TachyonExpressSignalingMessenger::ReceiveMessagesReader::OnDone(
    const grpc::Status& s) {
  {
    absl::MutexLock lock(mutex_);
    if (!is_receiving_messages_) {
      return;
    }
  }
  if (!s.ok()) {
    LOG(ERROR) << "ReceiveMessagesExpress failed: "
               << rpc::GrpcStatusToAbslStatus(s);
  }
  on_complete_callback_(s.ok());
}

void TachyonExpressSignalingMessenger::ReceiveMessagesReader::TryCancel() {
  {
    absl::MutexLock lock(mutex_);
    if (!is_receiving_messages_) {
      return;
    }
    is_receiving_messages_ = false;
  }
  context_.TryCancel();
}

TachyonExpressSignalingMessenger::TachyonExpressSignalingMessenger(
    absl::string_view self_id,
    const location::nearby::connections::LocationHint& location_hint)
    : self_id_(self_id),
      location_hint_(location_hint),
      account_manager_(AccountManagerImpl::Factory::instance()) {
  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel(std::string(kTachyonAddress),
                          grpc::SslCredentials(grpc::SslCredentialsOptions()));
  messaging_stub_ = google::internal::communications::instantmessaging::v1::
      grpc::Messaging::NewStub(channel);
}

struct StartState {
  CountDownLatch latch{1};
  bool success = false;
};

bool TachyonExpressSignalingMessenger::StartReceivingMessages(
    OnSignalingMessageCallback on_message_callback,
    OnSignalingCompleteCallback on_complete_callback) {
  auto state = std::make_shared<StartState>();

  account_manager_->GetAccessToken(
      [this, state, on_message_callback = std::move(on_message_callback),
       on_complete_callback = std::move(on_complete_callback)](
          absl::StatusOr<std::string> token) mutable {
        if (!token.ok()) {
          state->success = false;
          state->latch.CountDown();
          return;
        }
        auto reader = std::make_shared<ReceiveMessagesReader>();

        reader_ = reader;

        std::weak_ptr<StartState> weak_state = state;

        bool started = reader->Start(
            messaging_stub_.get(), self_id_, location_hint_, token.value(),
            /*on_fast_path_ready_callback=*/
            [state] {
              LOG(INFO) << "Received fast path ready message from tachyon.";
              state->success = true;
              state->latch.CountDown();
            },
            std::move(on_message_callback),
            [reader, weak_state,
             cb = std::move(on_complete_callback)](bool s) mutable {
              LOG(INFO) << "Finished receiving messages from tachyon.";
              cb(s);
              if (auto state = weak_state.lock()) {
                state->success = false;
                state->latch.CountDown();
              }
            });

        if (!started) {
          state->success = false;
          state->latch.CountDown();
        }
      });
  state->latch.Await();
  if (state->success) {
    LOG(INFO) << "Receiving messages from tachyon.";
  } else {
    LOG(ERROR) << "Failed to start receiving messages from tachyon.";
    reader_.reset();
  }
  return state->success;
}

void TachyonExpressSignalingMessenger::StopReceivingMessages() {
  if (reader_) {
    reader_->TryCancel();
    reader_.reset();
  }
}

bool TachyonExpressSignalingMessenger::SendMessage(absl::string_view peer_id,
                                                   const ByteArray& message) {
  auto rpc_state =
      std::make_shared<rpc::AsyncRpcArgs<SendMessageExpressRequest,
                                         SendMessageExpressResponse>>();

  InitRequestHeader(*rpc_state->request.mutable_header(), self_id_,
                    location_hint_);
  InitId(*rpc_state->request.mutable_dest_id(), peer_id, location_hint_);

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

  CountDownLatch latch(1);
  bool success = false;
  account_manager_->GetAccessToken(
      [this, &latch, &success, rpc_state](absl::StatusOr<std::string> token) {
        if (!token.ok()) {
          success = false;
          latch.CountDown();
          return;
        }

        const std::shared_ptr<grpc::CallCredentials> call_creds =
            grpc::AccessTokenCredentials(token.value());
        rpc_state->context.set_credentials(call_creds);
        gpr_timespec deadline = gpr_now(GPR_CLOCK_MONOTONIC);
        timespec timespec = absl::ToTimespec(absl::Seconds(30));
        deadline.tv_sec += timespec.tv_sec;
        deadline.tv_nsec += timespec.tv_nsec;
        rpc_state->context.set_deadline(deadline);

        // `rpc_state` is captured to ensure its lifetime is valid until the
        // callback is executed.
        messaging_stub_->async()->SendMessageExpress(
            &rpc_state->context, &rpc_state->request, &rpc_state->response,
            [&success, &latch, rpc_state](grpc::Status status) {
              if (!status.ok()) {
                LOG(ERROR) << "SendMessageExpress failed: "
                           << rpc::GrpcStatusToAbslStatus(status);
              }
              success = status.ok();
              latch.CountDown();
            });
      });
  latch.Await();
  return success;
}

}  // namespace nearby
