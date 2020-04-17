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

#include "core/internal/client_proxy.h"
#include "core/internal/endpoint_channel.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "securegcm/ukey2_handshake.h"

namespace location {
namespace nearby {
namespace connections {

// Encrypts a connection over UKEY2.
//
// <p>NOTE: Stalled EndpointChannels will be disconnected after {TIMEOUT_MILLIS}
// milliseconds. This is to prevent unverified endpoints from maintaining an
// indefinite connection to us.
template <typename Platform>
class EncryptionRunner {
 public:
  EncryptionRunner();
  ~EncryptionRunner();

  class ResultListener {
   public:
    virtual ~ResultListener() {}

    // @EncryptionRunnerThread
    virtual void onEncryptionSuccess(
        const string& endpoint_id,
        Ptr<securegcm::UKey2Handshake> ukey2_handshake,
        const string& authentication_token,
        ConstPtr<ByteArray> raw_authentication_token) = 0;

    // Encryption has failed. The remote_endpoint_id and channel are given so
    // that any pending state can be cleaned up.
    //
    // <p>We return the EndpointChannel because, at this stage, simultaneous
    // connections are a possibility. Use this channel to verify that the state
    // you're cleaning up is for this EndpointChannel, and not state for another
    // channel to the same endpoint.
    //
    // @EncryptionRunnerThread
    virtual void onEncryptionFailure(const string& endpoint_id,
                                     Ptr<EndpointChannel> channel) = 0;
  };

  // @AnyThread
  void startServer(Ptr<ClientProxy<Platform> > client_proxy,
                   const string& endpoint_id,
                   Ptr<EndpointChannel> endpoint_channel,
                   Ptr<ResultListener> result_listener);
  // @AnyThread
  void startClient(Ptr<ClientProxy<Platform> > client_proxy,
                   const string& endpoint_id,
                   Ptr<EndpointChannel> endpoint_channel,
                   Ptr<ResultListener> result_listener);

 private:
  ScopedPtr<Ptr<typename Platform::ScheduledExecutorType> > alarm_executor_;
  ScopedPtr<Ptr<typename Platform::SingleThreadExecutorType> > server_executor_;
  ScopedPtr<Ptr<typename Platform::SingleThreadExecutorType> > client_executor_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#include "core/internal/encryption_runner.cc"

#endif  // CORE_INTERNAL_ENCRYPTION_RUNNER_H_
