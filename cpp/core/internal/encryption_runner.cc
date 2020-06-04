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

#include "core/internal/encryption_runner.h"

#include <cinttypes>
#include <cstdint>

#include "platform/base64_utils.h"
#include "platform/byte_array.h"
#include "platform/cancelable_alarm.h"
#include "platform/exception.h"
#include "platform/logging.h"
#include "absl/strings/ascii.h"

namespace {

std::int64_t kTimeoutMillis = 15 * 1000;  // 15 seconds
std::int32_t kMaxUkey2VerificationStringLength = 32;
std::int32_t kTokenLength = 5;
securegcm::UKey2Handshake::HandshakeCipher kCipher =
    securegcm::UKey2Handshake::HandshakeCipher::P256_SHA512;

}  // namespace

namespace location {
namespace nearby {
namespace connections {

namespace {

// Transforms a raw UKEY2 token (which is a random ByteArray that's
// kMaxUkey2VerificationStringLength long) into a kTokenLength string that only
// uses A-Z0-9 for each character.
string toHumanReadableString(ConstPtr<ByteArray> token) {
  string result = Base64Utils::encode(token).substr(0, kTokenLength);
  absl::AsciiStrToUpper(&result);
  return result;
}

template <typename Platform>
bool handleEncryptionSuccess(
    const string& endpoint_id, Ptr<securegcm::UKey2Handshake> ukey2_handshake,
    Ptr<typename EncryptionRunner<Platform>::ResultListener> result_listener) {
  ScopedPtr<Ptr<securegcm::UKey2Handshake>> scoped_ukey2_handshake(
      ukey2_handshake);

  std::unique_ptr<string> verification_string =
      scoped_ukey2_handshake->GetVerificationString(
          kMaxUkey2VerificationStringLength);
  if (verification_string == nullptr) {
    return false;
  }

  ScopedPtr<ConstPtr<ByteArray>> raw_authentication_token(MakeConstPtr(
      new ByteArray(verification_string->data(), verification_string->size())));

  result_listener->onEncryptionSuccess(
      endpoint_id, scoped_ukey2_handshake.release(),
      toHumanReadableString(raw_authentication_token.get()),
      raw_authentication_token.release());

  return true;
}

template <typename Platform>
class CancelableAlarmRunnable : public Runnable {
 public:
  CancelableAlarmRunnable(Ptr<ClientProxy<Platform>> client_proxy,
                          const string& endpoint_id,
                          Ptr<EndpointChannel> endpoint_channel)
      : client_proxy_(client_proxy),
        endpoint_id_(endpoint_id),
        endpoint_channel_(endpoint_channel) {}

  void run() override {
    NEARBY_LOG(INFO,
               "Timing out encryption for client %" PRId64
               " to endpoint %s after %" PRId64 " ms",
               client_proxy_->getClientId(), endpoint_id_.c_str(),
               kTimeoutMillis);
    endpoint_channel_->close();
  }

 private:
  Ptr<ClientProxy<Platform>> client_proxy_;
  const string endpoint_id_;
  Ptr<EndpointChannel> endpoint_channel_;
};

template <typename Platform>
class ServerRunnable : public Runnable {
 public:
  ServerRunnable(Ptr<ClientProxy<Platform>> client_proxy,
                 Ptr<typename Platform::ScheduledExecutorType> alarm_executor,
                 const string& endpoint_id,
                 Ptr<EndpointChannel> endpoint_channel,
                 Ptr<typename EncryptionRunner<Platform>::ResultListener>
                     encryption_result_listener)
      : client_proxy_(client_proxy),
        alarm_executor_(alarm_executor),
        endpoint_id_(endpoint_id),
        endpoint_channel_(endpoint_channel),
        encryption_result_listener_(encryption_result_listener) {}

  void run() override {
    CancelableAlarm timeout_alarm(
        "EncryptionRunner.startServer() timeout",
        MakePtr(new CancelableAlarmRunnable<Platform>(
            client_proxy_, endpoint_id_, endpoint_channel_)),
        kTimeoutMillis, alarm_executor_);

    std::unique_ptr<securegcm::UKey2Handshake> server =
        securegcm::UKey2Handshake::ForResponder(kCipher);
    // Java code throws a HandshakeException.
    if (server == nullptr) {
      logException();
      handleHandshakeOrIOException(&timeout_alarm);
      return;
    }

    // Message 1 (Client Init)
    ExceptionOr<ConstPtr<ByteArray>> client_init = endpoint_channel_->read();
    if (!client_init.ok()) {
      if (Exception::IO == client_init.exception()) {
        logException();
        handleHandshakeOrIOException(&timeout_alarm);
        return;
      }
    }

    ScopedPtr<ConstPtr<ByteArray>> scoped_client_init(client_init.result());

    securegcm::UKey2Handshake::ParseResult parse_result =
        server->ParseHandshakeMessage(
            string(scoped_client_init->getData(), scoped_client_init->size()));

    // Java code throws a HandshakeException / AlertException.
    if (!parse_result.success) {
      logException();
      if (parse_result.alert_to_send != nullptr) {
        handleAlertException(parse_result);
      }
      handleHandshakeOrIOException(&timeout_alarm);
      return;
    }

    NEARBY_LOG(INFO, "In startServer(), read UKEY2 Message 1 from endpoint %s",
               endpoint_id_.c_str());

    // Message 2 (Server Init)
    std::unique_ptr<string> server_init = server->GetNextHandshakeMessage();

    // Java code throws a HandshakeException.
    if (server_init == nullptr) {
      logException();
      handleHandshakeOrIOException(&timeout_alarm);
      return;
    }

    Exception::Value write_exception = endpoint_channel_->write(
        MakeConstPtr(new ByteArray(server_init->data(), server_init->size())));
    if (Exception::NONE != write_exception) {
      if (Exception::IO == write_exception) {
        logException();
        handleHandshakeOrIOException(&timeout_alarm);
        return;
      }
    }

    NEARBY_LOG(INFO, "In startServer(), wrote UKEY2 Message 2 to endpoint %s",
               endpoint_id_.c_str());

    // Message 3 (Client Finish)
    ExceptionOr<ConstPtr<ByteArray>> client_finish = endpoint_channel_->read();

    if (!client_finish.ok()) {
      if (Exception::IO == client_finish.exception()) {
        logException();
        handleHandshakeOrIOException(&timeout_alarm);
        return;
      }
    }

    ScopedPtr<ConstPtr<ByteArray>> scoped_client_finish(client_finish.result());
    parse_result = server->ParseHandshakeMessage(
        string(scoped_client_finish->getData(), scoped_client_finish->size()));

    // Java code throws an AlertException or a HandshakeException.
    if (!parse_result.success) {
      logException();
      if (parse_result.alert_to_send != nullptr) {
        handleAlertException(parse_result);
      }
      handleHandshakeOrIOException(&timeout_alarm);
      return;
    }

    NEARBY_LOG(INFO, "In startServer(), read UKEY2 Message 3 from endpoint %s",
               endpoint_id_.c_str());

    timeout_alarm.cancel();

    if (!handleEncryptionSuccess<Platform>(endpoint_id_,
                                           MakePtr(server.release()),
                                           encryption_result_listener_.get())) {
      logException();
      handleHandshakeOrIOException(&timeout_alarm);
      return;
    }
  }

 private:
  void logException() {
    NEARBY_LOG(ERROR, "In startServer(), UKEY2 failed with endpoint %s",
               endpoint_id_.c_str());
  }

  void handleHandshakeOrIOException(CancelableAlarm* timeout_alarm) {
    timeout_alarm->cancel();
    encryption_result_listener_->onEncryptionFailure(endpoint_id_,
                                                     endpoint_channel_);
  }

  void handleAlertException(
      const securegcm::UKey2Handshake::ParseResult& parse_result) {
    Exception::Value write_exception = endpoint_channel_->write(
        MakeConstPtr(new ByteArray(parse_result.alert_to_send->data(),
                                   parse_result.alert_to_send->size())));
    if (Exception::NONE != write_exception) {
      if (Exception::IO == write_exception) {
        NEARBY_LOG(WARNING,
                   "In startServer(), client %" PRId64
                   " failed to pass the alert error message to endpoint %s",
                   client_proxy_->getClientId(), endpoint_id_.c_str());
      }
    }
  }

  Ptr<ClientProxy<Platform>> client_proxy_;
  Ptr<typename Platform::ScheduledExecutorType> alarm_executor_;
  const string endpoint_id_;
  Ptr<EndpointChannel> endpoint_channel_;
  ScopedPtr<Ptr<typename EncryptionRunner<Platform>::ResultListener>>
      encryption_result_listener_;
};

template <typename Platform>
class ClientRunnable : public Runnable {
 public:
  ClientRunnable(Ptr<ClientProxy<Platform>> client_proxy,
                 Ptr<typename Platform::ScheduledExecutorType> alarm_executor,
                 const string& endpoint_id,
                 Ptr<EndpointChannel> endpoint_channel,
                 Ptr<typename EncryptionRunner<Platform>::ResultListener>
                     encryption_result_listener)
      : client_proxy_(client_proxy),
        alarm_executor_(alarm_executor),
        endpoint_id_(endpoint_id),
        endpoint_channel_(endpoint_channel),
        encryption_result_listener_(encryption_result_listener) {}

  void run() override {
    CancelableAlarm timeout_alarm(
        "EncryptionRunner.startClient() timeout",
        MakePtr(new CancelableAlarmRunnable<Platform>(
            client_proxy_, endpoint_id_, endpoint_channel_)),
        kTimeoutMillis, alarm_executor_);

    std::unique_ptr<securegcm::UKey2Handshake> client =
        securegcm::UKey2Handshake::ForInitiator(kCipher);

    // Java code throws a HandshakeException.
    if (client == nullptr) {
      logException();
      handleHandshakeOrIOException(&timeout_alarm);
      return;
    }

    // Message 1 (Client Init)
    std::unique_ptr<string> client_init = client->GetNextHandshakeMessage();

    // Java code throws a HandshakeException.
    if (client_init == nullptr) {
      logException();
      handleHandshakeOrIOException(&timeout_alarm);
      return;
    }

    Exception::Value write_init_exception = endpoint_channel_->write(
        MakeConstPtr(new ByteArray(client_init->data(), client_init->size())));
    if (Exception::NONE != write_init_exception) {
      if (Exception::IO == write_init_exception) {
        logException();
        handleHandshakeOrIOException(&timeout_alarm);
        return;
      }
    }

    NEARBY_LOG(INFO, "In startClient(), wrote UKEY2 Message 1 to endpoint %s",
               endpoint_id_.c_str());

    // Message 2 (Server Init)
    ExceptionOr<ConstPtr<ByteArray>> server_init = endpoint_channel_->read();

    if (!server_init.ok()) {
      if (Exception::IO == server_init.exception()) {
        logException();
        handleHandshakeOrIOException(&timeout_alarm);
        return;
      }
    }

    ScopedPtr<ConstPtr<ByteArray>> scoped_server_init(server_init.result());
    securegcm::UKey2Handshake::ParseResult parse_result =
        client->ParseHandshakeMessage(
            string(scoped_server_init->getData(), scoped_server_init->size()));

    // Java code throws an AlertException or a HandshakeException.
    if (!parse_result.success) {
      logException();
      if (parse_result.alert_to_send != nullptr) {
        handleAlertException(parse_result);
      }
      handleHandshakeOrIOException(&timeout_alarm);
      return;
    }

    NEARBY_LOG(INFO, "In startClient(), read UKEY2 Message 2 from endpoint %s",
               endpoint_id_.c_str());

    // Message 3 (Client Finish)
    std::unique_ptr<string> client_finish = client->GetNextHandshakeMessage();

    // Java code throws a HandshakeException.
    if (client_finish == nullptr) {
      logException();
      handleHandshakeOrIOException(&timeout_alarm);
      return;
    }

    Exception::Value write_finish_exception =
        endpoint_channel_->write(MakeConstPtr(
            new ByteArray(client_finish->data(), client_finish->size())));
    if (Exception::NONE != write_finish_exception) {
      if (Exception::IO == write_finish_exception) {
        logException();
        handleHandshakeOrIOException(&timeout_alarm);
        return;
      }
    }

    NEARBY_LOG(INFO, "In startClient(), wrote UKEY2 Message 3 to endpoint %s",
               endpoint_id_.c_str());

    timeout_alarm.cancel();

    if (!handleEncryptionSuccess<Platform>(endpoint_id_,
                                           MakePtr(client.release()),
                                           encryption_result_listener_.get())) {
      logException();
      handleHandshakeOrIOException(&timeout_alarm);
      return;
    }
  }

 private:
  void logException() {
    NEARBY_LOG(ERROR, "In startClient(), UKEY2 failed with endpoint %s",
               endpoint_id_.c_str());
  }

  void handleHandshakeOrIOException(CancelableAlarm* timeout_alarm) {
    timeout_alarm->cancel();
    encryption_result_listener_->onEncryptionFailure(endpoint_id_,
                                                     endpoint_channel_);
  }

  void handleAlertException(
      const securegcm::UKey2Handshake::ParseResult& parse_result) {
    Exception::Value write_exception = endpoint_channel_->write(
        MakeConstPtr(new ByteArray(parse_result.alert_to_send->data(),
                                   parse_result.alert_to_send->size())));
    if (Exception::NONE != write_exception) {
      if (Exception::IO == write_exception) {
        NEARBY_LOG(WARNING,
                   "In startClient(), client %" PRId64
                   " failed to pass the alert error message to endpoint %s",
                   client_proxy_->getClientId(), endpoint_id_.c_str());
      }
    }
  }

  Ptr<ClientProxy<Platform>> client_proxy_;
  Ptr<typename Platform::ScheduledExecutorType> alarm_executor_;
  const string endpoint_id_;
  Ptr<EndpointChannel> endpoint_channel_;
  ScopedPtr<Ptr<typename EncryptionRunner<Platform>::ResultListener>>
      encryption_result_listener_;
};

}  // namespace

template <typename Platform>
EncryptionRunner<Platform>::EncryptionRunner()
    : alarm_executor_(Platform::createScheduledExecutor()),
      server_executor_(Platform::createSingleThreadExecutor()),
      client_executor_(Platform::createSingleThreadExecutor()) {}

template <typename Platform>
EncryptionRunner<Platform>::~EncryptionRunner() {
  // Stop all the ongoing Runnables (as gracefully as possible).
  client_executor_->shutdown();
  server_executor_->shutdown();
  alarm_executor_->shutdown();
}

template <typename Platform>
void EncryptionRunner<Platform>::startServer(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id,
    Ptr<EndpointChannel> endpoint_channel,
    Ptr<ResultListener> result_listener) {
  server_executor_->execute(MakePtr(new ServerRunnable<Platform>(
      client_proxy, alarm_executor_.get(), endpoint_id, endpoint_channel,
      result_listener)));
}

template <typename Platform>
void EncryptionRunner<Platform>::startClient(
    Ptr<ClientProxy<Platform>> client_proxy, const string& endpoint_id,
    Ptr<EndpointChannel> endpoint_channel,
    Ptr<ResultListener> result_listener) {
  client_executor_->execute(MakePtr(new ClientRunnable<Platform>(
      client_proxy, alarm_executor_.get(), endpoint_id, endpoint_channel,
      result_listener)));
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
