// Copyright 2021 Google LLC
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
#include "third_party/nearby_connections/windows/listeners.h"

#include "core/listeners.h"
#include "core/params.h"

namespace location {
namespace nearby {
namespace windows {

const char* ConnectionResponseInfoFP::GetAuthenticationDigits() {
  ByteUtils::ToFourDigitCharString(raw_authentication_token,
                                   four_digit_char_string_);

  return four_digit_char_string_;
}

__stdcall ResultCallbackFP::ResultCallbackFP()
    : impl_(new connections::ResultCallback(),
            [](connections::ResultCallback* impl) { delete impl; }) {
  impl_->result_cb = result_cb;
}

ConnectionResponseInfoFP::ConnectionResponseInfoFP(
    const connections::ConnectionResponseInfo* connection_response_info)
    : remote_endpoint_info(connection_response_info->remote_endpoint_info),
      authentication_token(connection_response_info->authentication_token),
      raw_authentication_token(
          connection_response_info->raw_authentication_token),
      is_incoming_connection(connection_response_info->is_incoming_connection),
      is_connection_verified(connection_response_info->is_connection_verified),
      four_digit_char_string_{0} {}

ConnectionListenerFP::ConnectionListenerFP()
    : impl_(new connections::ConnectionListener(),
            [](connections::ConnectionListener* impl) { delete impl; }) {
  impl_->initiated_cb = std::function(
      [this](
          const std::string& endpoint_id,
          const connections::ConnectionResponseInfo& connection_response_info) {
        initiated_cb(endpoint_id.c_str(),
                     ConnectionResponseInfoFP(&connection_response_info));
      });

  impl_->accepted_cb = std::function([this](const std::string& endpoint_id) {
    accepted_cb(endpoint_id.c_str());
  });

  impl_->rejected_cb = std::function(
      [this](const std::string& endpoint_id, connections::Status status) {
        rejected_cb(endpoint_id.c_str(), status);
      });

  impl_->disconnected_cb =
      std::function([this](const std::string& endpoint_id) {
        disconnected_cb(endpoint_id.c_str());
      });

  impl_->bandwidth_changed_cb =
      std::function([this](const std::string& endpoint_id, Medium medium) {
        bandwidth_changed_cb(endpoint_id.c_str(), medium);
      });
}

ConnectionListenerFP::ConnectionListenerFP(
    connections::ConnectionListener connection_listener)
    : impl_(new connections::ConnectionListener(),
            [](connections::ConnectionListener* impl) { delete impl; }) {
  connection_listener.initiated_cb = std::function(
      [this](
          const std::string& endpoint_id,
          const connections::ConnectionResponseInfo& connection_response_info) {
        initiated_cb(endpoint_id.c_str(),
                     ConnectionResponseInfoFP(&connection_response_info));
      });

  connection_listener.accepted_cb =
      std::function([this](const std::string& endpoint_id) {
        accepted_cb(endpoint_id.c_str());
      });

  connection_listener.rejected_cb = std::function(
      [this](const std::string& endpoint_id, connections::Status status) {
        rejected_cb(endpoint_id.c_str(), status);
      });

  connection_listener.disconnected_cb =
      std::function([this](const std::string& endpoint_id) {
        disconnected_cb(endpoint_id.c_str());
      });

  connection_listener.bandwidth_changed_cb = std::function(
      [this](const std::string& endpoint_id, connections::Medium medium) {
        bandwidth_changed_cb(endpoint_id.c_str(), medium);
      });
}

ConnectionListenerFP::operator connections::ConnectionListener() const {
  return *impl_.get();
}

DiscoveryListenerFP::DiscoveryListenerFP()
    : impl_(new connections::DiscoveryListener(),
            [](connections::DiscoveryListener* impl) { delete impl; }) {
  impl_->endpoint_found_cb = std::function(
      [this](std::string endpoint_id, const ByteArray& endpoint_info,
             std::string service_id) {
        endpoint_found_cb(endpoint_id.c_str(), endpoint_info,
                          service_id.c_str());
      });

  impl_->endpoint_lost_cb = std::function([this](std::string endpoint_id) {
    endpoint_lost_cb(endpoint_id.c_str());
  });

  impl_->endpoint_distance_changed_cb = std::function(
      [this](std::string endpoint_id, connections::DistanceInfo distance_info) {
        endpoint_distance_changed_cb(endpoint_id.c_str(), distance_info);
      });
}

DiscoveryListenerFP::operator connections::DiscoveryListener() const {
  return *impl_.get();
}

PayloadListenerFP::PayloadListenerFP()
    : impl_(new connections::PayloadListener(),
            [](connections::PayloadListener* impl) { delete impl; }) {
  impl_->payload_cb = std::function(
      [this](const std::string& endpoint_id, connections::Payload payload) {
        payload_cb(endpoint_id.c_str(), payload);
      });

  impl_->payload_progress_cb = std::function(
      [this](const std::string& endpoint_id,
             const connections::PayloadProgressInfo& payload_progress_info) {
        payload_progress_cb(endpoint_id.c_str(), payload_progress_info);
      });
}

PayloadListenerFP::operator connections::PayloadListener() const {
  return *impl_.get();
}

ConnectionRequestInfoFP::ConnectionRequestInfoFP(
    const connections::ConnectionRequestInfo* connection_request_info)
    : endpoint_info(connection_request_info->endpoint_info),
      listener(connection_request_info->listener) {}

ConnectionRequestInfoFP::operator connections::ConnectionRequestInfo() const {
  return {
      .endpoint_info = endpoint_info,
      .listener = listener,
  };
}

ConnectionResponseInfoFP::operator connections::ConnectionResponseInfo() const {
  return {.remote_endpoint_info = remote_endpoint_info,
          .authentication_token = authentication_token,
          .raw_authentication_token = raw_authentication_token,
          .is_incoming_connection = is_incoming_connection,
          .is_connection_verified = is_connection_verified};
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
