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

#include "connections/implementation/encryption_runner.h"

#include <cinttypes>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "securegcm/ukey2_handshake.h"
#include "absl/strings/ascii.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/cancelable_alarm.h"
#include "internal/platform/exception.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace connections {
namespace {

constexpr absl::Duration kTimeout = absl::Seconds(15);
constexpr std::int32_t kMaxUkey2VerificationStringLength = 32;
constexpr std::int32_t kTokenLength = 5;
constexpr securegcm::UKey2Handshake::HandshakeCipher kCipher =
    securegcm::UKey2Handshake::HandshakeCipher::P256_SHA512;

// Transforms a raw UKEY2 token (which is a random ByteArray that's
// kMaxUkey2VerificationStringLength long) into a kTokenLength string that only
// uses [A-Z], [0-9], '_', '-' for each character.
std::string ToHumanReadableString(const ByteArray& token) {
  std::string result = Base64Utils::Encode(token).substr(0, kTokenLength);
  absl::AsciiStrToUpper(&result);
  return result;
}

bool HandleEncryptionSuccess(const std::string& endpoint_id,
                             std::unique_ptr<securegcm::UKey2Handshake> ukey2,
                             EncryptionRunner::ResultListener& listener) {
  std::unique_ptr<std::string> verification_string =
      ukey2->GetVerificationString(kMaxUkey2VerificationStringLength);
  if (verification_string == nullptr) {
    return false;
  }

  ByteArray raw_authentication_token(*verification_string);

  listener.CallSuccessCallback(endpoint_id, std::move(ukey2),
                               ToHumanReadableString(raw_authentication_token),
                               raw_authentication_token);

  return true;
}

void CancelableAlarmRunnable(ClientProxy* client,
                             const std::string& endpoint_id,
                             EndpointChannel* endpoint_channel) {
  NEARBY_LOGS(INFO) << "Timing out encryption for client "
                    << client->GetClientId()
                    << " to endpoint_id=" << endpoint_id << " after "
                    << absl::FormatDuration(kTimeout);
  endpoint_channel->Close();
}

class ServerRunnable final {
 public:
  ServerRunnable(ClientProxy* client, ScheduledExecutor* alarm_executor,
                 const std::string& endpoint_id, EndpointChannel* channel,
                 EncryptionRunner::ResultListener listener)
      : client_(client),
        alarm_executor_(alarm_executor),
        endpoint_id_(endpoint_id),
        channel_(channel),
        listener_(std::move(listener)) {}

  void operator()() {
    CancelableAlarm timeout_alarm(
        "EncryptionRunner.StartServer() timeout",
        [this]() { CancelableAlarmRunnable(client_, endpoint_id_, channel_); },
        kTimeout, alarm_executor_);

    std::unique_ptr<securegcm::UKey2Handshake> server =
        securegcm::UKey2Handshake::ForResponder(kCipher);
    if (server == nullptr) {
      LogException();
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }

    // Message 1 (Client Init)
    ExceptionOr<ByteArray> client_init = channel_->Read();
    if (!client_init.ok()) {
      LogException();
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }

    securegcm::UKey2Handshake::ParseResult parse_result =
        server->ParseHandshakeMessage(std::string(client_init.result()));

    // Java code throws a HandshakeException / AlertException.
    if (!parse_result.success) {
      LogException();
      if (parse_result.alert_to_send != nullptr) {
        HandleAlertException(parse_result);
      }
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }

    NEARBY_LOGS(INFO)
        << "In StartServer(), read UKEY2 Message 1 from endpoint(id="
        << endpoint_id_ << ").";

    // Message 2 (Server Init)
    std::unique_ptr<std::string> server_init =
        server->GetNextHandshakeMessage();

    // Java code throws a HandshakeException.
    if (server_init == nullptr) {
      LogException();
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }

    Exception write_exception =
        channel_->Write(ByteArray(std::move(*server_init)));
    if (!write_exception.Ok()) {
      LogException();
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }

    NEARBY_LOGS(INFO)
        << "In StartServer(), wrote UKEY2 Message 2 to endpoint(id="
        << endpoint_id_ << ").";

    // Message 3 (Client Finish)
    ExceptionOr<ByteArray> client_finish = channel_->Read();

    if (!client_finish.ok()) {
      LogException();
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }

    parse_result =
        server->ParseHandshakeMessage(std::string(client_finish.result()));

    // Java code throws an AlertException or a HandshakeException.
    if (!parse_result.success) {
      LogException();
      if (parse_result.alert_to_send != nullptr) {
        HandleAlertException(parse_result);
      }
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }

    NEARBY_LOGS(INFO)
        << "In StartServer(), read UKEY2 Message 3 from endpoint(id="
        << endpoint_id_ << ").";

    timeout_alarm.Cancel();

    if (!HandleEncryptionSuccess(endpoint_id_, std::move(server), listener_)) {
      LogException();
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }
  }

 private:
  void LogException() const {
    NEARBY_LOGS(ERROR) << "In StartServer(), UKEY2 failed with endpoint(id="
                       << endpoint_id_ << ").";
  }

  void HandleHandshakeOrIoException(CancelableAlarm* timeout_alarm) {
    timeout_alarm->Cancel();
    listener_.CallFailureCallback(endpoint_id_, channel_);
  }

  void HandleAlertException(
      const securegcm::UKey2Handshake::ParseResult& parse_result) const {
    Exception write_exception =
        channel_->Write(ByteArray(*parse_result.alert_to_send));
    if (!write_exception.Ok()) {
      NEARBY_LOGS(WARNING)
          << "In StartServer(), client " << client_->GetClientId()
          << " failed to pass the alert error message to endpoint(id="
          << endpoint_id_ << ").";
    }
  }

  ClientProxy* client_;
  ScheduledExecutor* alarm_executor_;
  const std::string endpoint_id_;
  EndpointChannel* channel_;
  EncryptionRunner::ResultListener listener_;
};

class ClientRunnable final {
 public:
  ClientRunnable(ClientProxy* client, ScheduledExecutor* alarm_executor,
                 const std::string& endpoint_id, EndpointChannel* channel,
                 EncryptionRunner::ResultListener listener)
      : client_(client),
        alarm_executor_(alarm_executor),
        endpoint_id_(endpoint_id),
        channel_(channel),
        listener_(std::move(listener)) {}

  void operator()() {
    CancelableAlarm timeout_alarm(
        "EncryptionRunner.StartClient() timeout",
        [this]() { CancelableAlarmRunnable(client_, endpoint_id_, channel_); },
        kTimeout, alarm_executor_);

    std::unique_ptr<securegcm::UKey2Handshake> crypto =
        securegcm::UKey2Handshake::ForInitiator(kCipher);

    // Java code throws a HandshakeException.
    if (crypto == nullptr) {
      LogException();
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }

    // Message 1 (Client Init)
    std::unique_ptr<std::string> client_init =
        crypto->GetNextHandshakeMessage();

    // Java code throws a HandshakeException.
    if (client_init == nullptr) {
      LogException();
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }

    Exception write_init_exception = channel_->Write(ByteArray(*client_init));
    if (!write_init_exception.Ok()) {
      LogException();
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }

    NEARBY_LOGS(INFO)
        << "In StartClient(), wrote UKEY2 Message 1 to endpoint(id="
        << endpoint_id_ << ").";

    // Message 2 (Server Init)
    ExceptionOr<ByteArray> server_init = channel_->Read();

    if (!server_init.ok()) {
      LogException();
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }

    securegcm::UKey2Handshake::ParseResult parse_result =
        crypto->ParseHandshakeMessage(std::string(server_init.result()));

    // Java code throws an AlertException or a HandshakeException.
    if (!parse_result.success) {
      LogException();
      if (parse_result.alert_to_send != nullptr) {
        HandleAlertException(parse_result);
      }
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }

    NEARBY_LOGS(INFO)
        << "In StartClient(), read UKEY2 Message 2 from endpoint(id="
        << endpoint_id_ << ").";

    // Message 3 (Client Finish)
    std::unique_ptr<std::string> client_finish =
        crypto->GetNextHandshakeMessage();

    // Java code throws a HandshakeException.
    if (client_finish == nullptr) {
      LogException();
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }

    Exception write_finish_exception =
        channel_->Write(ByteArray(*client_finish));
    if (!write_finish_exception.Ok()) {
      LogException();
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }

    NEARBY_LOGS(INFO)
        << "In StartClient(), wrote UKEY2 Message 3 to endpoint(id="
        << endpoint_id_ << ").";

    timeout_alarm.Cancel();

    if (!HandleEncryptionSuccess(endpoint_id_, std::move(crypto), listener_)) {
      LogException();
      HandleHandshakeOrIoException(&timeout_alarm);
      return;
    }
  }

 private:
  void LogException() const {
    NEARBY_LOGS(ERROR) << "In StartClient(), UKEY2 failed with endpoint(id="
                       << endpoint_id_ << ").";
  }

  void HandleHandshakeOrIoException(CancelableAlarm* timeout_alarm) {
    timeout_alarm->Cancel();
    listener_.CallFailureCallback(endpoint_id_, channel_);
  }

  void HandleAlertException(
      const securegcm::UKey2Handshake::ParseResult& parse_result) const {
    Exception write_exception =
        channel_->Write(ByteArray(*parse_result.alert_to_send));
    if (!write_exception.Ok()) {
      NEARBY_LOGS(WARNING)
          << "In StartClient(), client " << client_->GetClientId()
          << " failed to pass the alert error message to endpoint(id="
          << endpoint_id_ << ").";
    }
  }

  ClientProxy* client_;
  ScheduledExecutor* alarm_executor_;
  const std::string endpoint_id_;
  EndpointChannel* channel_;
  EncryptionRunner::ResultListener listener_;
};

}  // namespace

EncryptionRunner::~EncryptionRunner() {
  // Stop all the ongoing Runnables (as gracefully as possible).
  client_executor_.Shutdown();
  server_executor_.Shutdown();
  alarm_executor_.Shutdown();
}

void EncryptionRunner::StartServer(ClientProxy* client,
                                   const std::string& endpoint_id,
                                   EndpointChannel* endpoint_channel,
                                   EncryptionRunner::ResultListener listener) {
  ServerRunnable runnable(client, &alarm_executor_, endpoint_id,
                          endpoint_channel, std::move(listener));
  server_executor_.Execute("encryption-server", std::move(runnable));
}

void EncryptionRunner::StartClient(ClientProxy* client,
                                   const std::string& endpoint_id,
                                   EndpointChannel* endpoint_channel,
                                   EncryptionRunner::ResultListener listener) {
  ClientRunnable runnable(client, &alarm_executor_, endpoint_id,
                          endpoint_channel, std::move(listener));
  client_executor_.Execute("encryption-client", std::move(runnable));
}

void EncryptionRunner::ResultListener::CallSuccessCallback(
    const std::string& endpoint_id,
    std::unique_ptr<securegcm::UKey2Handshake> ukey2,
    const std::string& auth_token, const ByteArray& raw_auth_token) {
  if (on_success_cb) {
    std::move(on_success_cb)(endpoint_id, std::move(ukey2), auth_token,
                             raw_auth_token);
  }
  Reset();
}

void EncryptionRunner::ResultListener::CallFailureCallback(
    const std::string& endpoint_id, EndpointChannel* channel) {
  if (on_failure_cb) {
    std::move(on_failure_cb)(endpoint_id, channel);
  }
  Reset();
}

void EncryptionRunner::ResultListener::Reset() {
  on_success_cb = nullptr;
  on_failure_cb = nullptr;
}

}  // namespace connections
}  // namespace nearby
