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

#ifndef NO_WEBRTC

#include <grpcpp/security/credentials.h>

#include <memory>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "third_party/grpc/include/grpcpp/client_context.h"
#include "third_party/grpc/include/grpcpp/support/client_callback.h"
#include "third_party/grpc/include/grpcpp/support/status.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/account_manager.h"
#include "internal/platform/implementation/webrtc.h"
#include "internal/proto/messaging.grpc.pb.h"

namespace nearby {

// Interface for the messaging Tachyon service. See
// third_party/nearby/internal/proto/messaging.proto
class TachyonExpressSignalingMessenger : public api::WebRtcSignalingMessenger {
 public:
  explicit TachyonExpressSignalingMessenger(
      absl::string_view self_id,
      const location::nearby::connections::LocationHint& location_hint);

  class ReceiveMessagesReader
      : public grpc::ClientReadReactor<
            google::internal::communications::instantmessaging::v1::
                ReceiveMessagesResponse> {
   public:
    ReceiveMessagesReader() = default;

    void OnReadDone(bool ok) override;
    void OnDone(const grpc::Status& s) override;

    bool Start(
        google::internal::communications::instantmessaging::v1::grpc::
            Messaging::StubInterface* stub,
        absl::string_view self_id,
        const location::nearby::connections::LocationHint& location_hint,
        absl::string_view access_token,
        absl::AnyInvocable<void()> on_fast_path_ready_callback,
        absl::AnyInvocable<void(const ByteArray&)> on_inbox_message_callback,
        absl::AnyInvocable<void(bool)> on_complete_callback);
    void TryCancel();

   private:
    grpc::ClientContext context_;
    google::internal::communications::instantmessaging::v1::
        ReceiveMessagesExpressRequest request_;
    google::internal::communications::instantmessaging::v1::
        ReceiveMessagesResponse response_;
    absl::AnyInvocable<void()> on_fast_path_ready_callback_;
    absl::AnyInvocable<void(const ByteArray&)> on_inbox_message_callback_;
    absl::AnyInvocable<void(bool)> on_complete_callback_;

    absl::Mutex mutex_;
    bool is_receiving_messages_ ABSL_GUARDED_BY(mutex_) = false;
  };

  bool SendMessage(absl::string_view peer_id,
                   const ByteArray& message) override;

  bool StartReceivingMessages(
      OnSignalingMessageCallback on_message_callback,
      OnSignalingCompleteCallback on_complete_callback) override;

  void StopReceivingMessages() override;

 private:
  std::string self_id_;
  location::nearby::connections::LocationHint location_hint_;
  std::unique_ptr<google::internal::communications::instantmessaging::v1::grpc::
                      Messaging::StubInterface>
      messaging_stub_;
  AccountManager* const account_manager_;
  std::shared_ptr<ReceiveMessagesReader> reader_ = nullptr;
};

}  // namespace nearby

#endif  // #ifndef NO_WEBRTC

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_TACHYON_MESSAGING_CLIENT_H_
