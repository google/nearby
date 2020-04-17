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

#ifndef CORE_LISTENERS_H_
#define CORE_LISTENERS_H_

#include <cstdint>

#include "core/payload.h"
#include "core/status.h"
#include "platform/byte_array.h"
#include "platform/port/string.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {
namespace connections {

struct OnConnectionInitiatedParams {
  const std::string remote_endpoint_id;
  const std::string remote_endpoint_name;
  const std::string authentication_token;
  ConstPtr<ByteArray> raw_authentication_token;
  const bool is_incoming_connection;

  OnConnectionInitiatedParams(const std::string& remote_endpoint_id,
                              const std::string& remote_endpoint_name,
                              const std::string& authentication_token,
                              ConstPtr<ByteArray> raw_authentication_token,
                              bool is_incoming_connection)
      : remote_endpoint_id(remote_endpoint_id),
        remote_endpoint_name(remote_endpoint_name),
        authentication_token(authentication_token),
        raw_authentication_token(raw_authentication_token),
        is_incoming_connection(is_incoming_connection) {}
};

struct OnConnectionResultParams {
  const std::string remote_endpoint_id;
  const Status::Value status;

  OnConnectionResultParams(const std::string& remote_endpoint_id,
                           Status::Value status)
      : remote_endpoint_id(remote_endpoint_id), status(status) {}
};

struct OnDisconnectedParams {
  const std::string remote_endpoint_id;

  explicit OnDisconnectedParams(const std::string& remote_endpoint_id)
      : remote_endpoint_id(remote_endpoint_id) {}
};

struct OnBandwidthChangedParams {
  const std::string remote_endpoint_id;
  const std::int32_t quality;

  OnBandwidthChangedParams(const std::string& remote_endpoint_id,
                           std::int32_t quality)
      : remote_endpoint_id(remote_endpoint_id), quality(quality) {}
};

struct OnPayloadReceivedParams {
  const std::string remote_endpoint_id;
  const ConstPtr<Payload> payload;

  OnPayloadReceivedParams(const std::string& remote_endpoint_id,
                          ConstPtr<Payload> payload)
      : remote_endpoint_id(remote_endpoint_id), payload(payload) {}
};

struct PayloadTransferUpdate {
  const std::int64_t payload_id;
  struct Status {
    enum Value {
      SUCCESS,
      FAILURE,
      IN_PROGRESS,
      CANCELED,
    };
  };
  const Status::Value status;
  const std::int64_t total_bytes;
  const std::int64_t bytes_transferred;

  PayloadTransferUpdate(std::int64_t payload_id, Status::Value status,
                        std::int64_t total_bytes,
                        std::int64_t bytes_transferred)
      : payload_id(payload_id),
        status(status),
        total_bytes(total_bytes),
        bytes_transferred(bytes_transferred) {}
};

struct OnPayloadTransferUpdateParams {
  const std::string remote_endpoint_id;
  const PayloadTransferUpdate update;

  OnPayloadTransferUpdateParams(const std::string& remote_endpoint_id,
                                const PayloadTransferUpdate& update)
      : remote_endpoint_id(remote_endpoint_id), update(update) {}
};

struct OnEndpointFoundParams {
  const std::string endpoint_id;
  const std::string service_id;
  const std::string endpoint_name;

  OnEndpointFoundParams(const std::string& endpoint_id,
                        const std::string& service_id,
                        const std::string& endpoint_name)
      : endpoint_id(endpoint_id),
        service_id(service_id),
        endpoint_name(endpoint_name) {}
};

struct OnEndpointLostParams {
  const std::string endpoint_id;

  explicit OnEndpointLostParams(const std::string& endpoint_id)
      : endpoint_id(endpoint_id) {}
};

class ResultListener {
 public:
  virtual ~ResultListener() {}

  virtual void onResult(Status::Value status) = 0;
};

class ConnectionLifecycleListener {
 public:
  virtual ~ConnectionLifecycleListener() {}

  virtual void onConnectionInitiated(
      ConstPtr<OnConnectionInitiatedParams> on_connection_initiated_params) = 0;
  virtual void onConnectionResult(
      ConstPtr<OnConnectionResultParams> on_connection_result_params) = 0;
  virtual void onDisconnected(
      ConstPtr<OnDisconnectedParams> on_disconnected_params) = 0;
  virtual void onBandwidthChanged(
      ConstPtr<OnBandwidthChangedParams> on_bandwidth_changed_params) = 0;
};

class DiscoveryListener {
 public:
  virtual ~DiscoveryListener() {}

  virtual void onEndpointFound(
      ConstPtr<OnEndpointFoundParams> on_endpoint_found_params) = 0;
  virtual void onEndpointLost(
      ConstPtr<OnEndpointLostParams> on_endpoint_lost_params) = 0;
};

class PayloadListener {
 public:
  virtual ~PayloadListener() {}

  virtual void onPayloadReceived(
      ConstPtr<OnPayloadReceivedParams> on_payload_received_params) = 0;
  virtual void onPayloadTransferUpdate(
      ConstPtr<OnPayloadTransferUpdateParams>
          on_payload_transfer_update_params) = 0;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_LISTENERS_H_
