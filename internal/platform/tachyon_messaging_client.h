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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_TACHYON_MESSAGING_CLIENT_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_TACHYON_MESSAGING_CLIENT_H_

#include <grpcpp/security/credentials.h>

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "third_party/grpc/include/grpcpp/client_context.h"
#include "third_party/grpc/include/grpcpp/security/credentials.h"
#include "third_party/grpc/include/grpcpp/support/client_callback.h"
#include "third_party/grpc/include/grpcpp/support/status.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/webrtc.h"
#include "internal/proto/messaging.grpc.pb.h"

namespace nearby {

// Interface for the messaging Tachyon service. See
// third_party/nearby/internal/proto/messaging.proto
class TachyonMessagingClient {
 public:
  TachyonMessagingClient();
  ~TachyonMessagingClient();

  class ReceiveMessagesReader
      : public grpc::ClientReadReactor<
            google::internal::communications::instantmessaging::v1::
                ReceiveMessagesResponse> {
   public:
    ReceiveMessagesReader(
        google::internal::communications::instantmessaging::v1::grpc::
            Messaging::StubInterface* stub,
        absl::string_view self_id, absl::string_view access_token,
        absl::string_view location_hint,
        api::WebRtcSignalingMessenger::OnSignalingMessageCallback
            on_message_callback,
        api::WebRtcSignalingMessenger::OnSignalingCompleteCallback
            on_complete_callback);

    void OnReadDone(bool ok) override;
    void OnDone(const grpc::Status& s) override;

    void TryCancel();

   private:
    grpc::ClientContext context_;
    google::internal::communications::instantmessaging::v1::
        ReceiveMessagesExpressRequest request_;
    google::internal::communications::instantmessaging::v1::
        ReceiveMessagesResponse response_;
    api::WebRtcSignalingMessenger::OnSignalingMessageCallback
        on_message_callback_;
    api::WebRtcSignalingMessenger::OnSignalingCompleteCallback
        on_complete_callback_;
  };

  void ReceiveMessagesExpress(
      absl::string_view self_id, absl::string_view access_token,
      absl::string_view location_hint,
      api::WebRtcSignalingMessenger::OnSignalingMessageCallback
          on_message_callback,
      api::WebRtcSignalingMessenger::OnSignalingCompleteCallback
          on_complete_callback);

  void StopReceivingMessages();

  void SendMessageExpress(
      absl::string_view self_id, absl::string_view peer_id,
      const ByteArray& message, absl::string_view access_token,
      absl::string_view location_hint,
      absl::AnyInvocable<void(absl::Status response)> callback);

  void SetMessagingStubForTest(
      std::unique_ptr<google::internal::communications::instantmessaging::v1::
                          grpc::Messaging::StubInterface>
          messaging_stub) {
    messaging_stub_ = std::move(messaging_stub);
  }

 private:
  std::shared_ptr<grpc::ChannelCredentials> CreateChannelCredentials();

  std::unique_ptr<google::internal::communications::instantmessaging::v1::grpc::
                      Messaging::StubInterface>
      messaging_stub_;
  std::unique_ptr<ReceiveMessagesReader> reader_ = nullptr;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_TACHYON_MESSAGING_CLIENT_H_
