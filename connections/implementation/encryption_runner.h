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

#ifndef CORE_INTERNAL_ENCRYPTION_RUNNER_H_
#define CORE_INTERNAL_ENCRYPTION_RUNNER_H_

#include <string>

#include "securegcm/ukey2_handshake.h"
#include "connections/implementation/client_proxy.h"
#include "connections/implementation/endpoint_channel.h"
#include "connections/listeners.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/scheduled_executor.h"
#include "internal/platform/single_thread_executor.h"

namespace nearby {
namespace connections {

// Encrypts a connection over UKEY2.
//
// NOTE: Stalled EndpointChannels will be disconnected after kTimeout.
// This is to prevent unverified endpoints from maintaining an
// indefinite connection to us.
class EncryptionRunner {
 public:
  EncryptionRunner() = default;
  ~EncryptionRunner();

  struct ResultListener {
    // @EncryptionRunnerThread
    std::function<void(const std::string& endpoint_id,
                       std::unique_ptr<securegcm::UKey2Handshake> ukey2,
                       const std::string& auth_token,
                       const ByteArray& raw_auth_token)>
        on_success_cb = [](const std::string&,
                           std::unique_ptr<securegcm::UKey2Handshake>,
                           const std::string&, const ByteArray&) {};

    // Encryption has failed. The remote_endpoint_id and channel are given so
    // that any pending state can be cleaned up.
    //
    // We return the EndpointChannel because, at this stage, simultaneous
    // connections are a possibility. Use this channel to verify that the state
    // you're cleaning up is for this EndpointChannel, and not state for another
    // channel to the same endpoint.
    //
    // @EncryptionRunnerThread
    std::function<void(const std::string& endpoint_id,
                       EndpointChannel* channel)>
        on_failure_cb = [](const std::string&, EndpointChannel*) {};
  };

  // @AnyThread
  void StartServer(ClientProxy* client, const std::string& endpoint_id,
                   EndpointChannel* endpoint_channel,
                   ResultListener&& result_listener);
  // @AnyThread
  void StartClient(ClientProxy* client, const std::string& endpoint_id,
                   EndpointChannel* endpoint_channel,
                   ResultListener&& result_listener);

 private:
  ScheduledExecutor alarm_executor_;
  SingleThreadExecutor server_executor_;
  SingleThreadExecutor client_executor_;
};

}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_ENCRYPTION_RUNNER_H_
