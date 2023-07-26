// Copyright 2022 Google LLC
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

#include <memory>
#include <utility>

#include "connections/c/listeners_w.h"

#include "connections/listeners.h"

namespace nearby {
// Must implement Deleters, since the connections classes weren't
// fully defined in the header
namespace connections {
void ConnectionListenerDeleter::operator()(connections::ConnectionListener *p) {
  delete p;
}
void DiscoveryListenerDeleter::operator()(connections::DiscoveryListener *p) {
  delete p;
}
void PayloadListenerDeleter::operator()(connections::PayloadListener *p) {
  delete p;
}
}  // namespace connections

namespace windows {

static ResultCallbackW *ResultCallbackImpl;

void ResultCB(Status status) { ResultCallbackImpl->result_cb(status); }

ResultCallbackW::ResultCallbackW()
    : impl(std::make_unique<connections::ResultCallback>(ResultCB)) {
  ResultCallbackImpl = this;
}

ResultCallbackW::~ResultCallbackW() {}

ResultCallbackW::ResultCallbackW(ResultCallbackW &other) {
  impl = std::move(other.impl);
}

ResultCallbackW::ResultCallbackW(ResultCallbackW &&other) noexcept {
  impl = std::move(other.impl);
}

ConnectionListenerW::ConnectionListenerW(InitiatedCB initiatedCB,
                                         AcceptedCB acceptedCB,
                                         RejectedCB rejectedCB,
                                         DisconnectedCB disconnectedCB,
                                         BandwidthChangedCB bandwidthChangedCB)
    : initiated_cb(initiatedCB),
      accepted_cb(acceptedCB),
      rejected_cb(rejectedCB),
      disconnected_cb(disconnectedCB),
      bandwidth_changed_cb(bandwidthChangedCB),
      impl_(std::unique_ptr<connections::ConnectionListener,
                            connections::ConnectionListenerDeleter>(
          new connections::ConnectionListener())) {
  CHECK(initiated_cb != nullptr);
  auto i = initiated_cb;
  impl_->initiated_cb =
      [i](const std::string &endpoint_id,
          const connections::ConnectionResponseInfo connection_response_info) {
        ConnectionResponseInfoW connection_response_info_w{
            connection_response_info.remote_endpoint_info.data(),
            connection_response_info.remote_endpoint_info.size(),
            connection_response_info.authentication_token.c_str(),
            connection_response_info.raw_authentication_token.data(),
            connection_response_info.raw_authentication_token.size(),
            connection_response_info.is_incoming_connection,
            connection_response_info.is_connection_verified};
        i(endpoint_id.c_str(), connection_response_info_w);
      };

  CHECK(accepted_cb != nullptr);
  auto a = accepted_cb;
  impl_->accepted_cb = [a](const std::string &endpoint_id) {
    a(endpoint_id.c_str());
  };

  CHECK(rejected_cb != nullptr);
  auto r = rejected_cb;
  impl_->rejected_cb = [r](const std::string &endpoint_id, Status status) {
    r(endpoint_id.c_str(), status);
  };

  CHECK(disconnected_cb != nullptr);
  auto d = disconnected_cb;
  impl_->disconnected_cb = [d](const std::string &endpoint_id) {
    d(endpoint_id.c_str());
  };

  CHECK(bandwidth_changed_cb != nullptr);
  auto bwc = bandwidth_changed_cb;
  impl_->bandwidth_changed_cb = [bwc](const std::string &endpoint_id,
                                      connections::Medium medium) {
    bwc(endpoint_id.c_str(), medium);
  };
}

ConnectionListenerW::ConnectionListenerW(ConnectionListenerW &other) {
  impl_ = std::move(other.impl_);
  accepted_cb = other.accepted_cb;
  bandwidth_changed_cb = other.bandwidth_changed_cb;
  disconnected_cb = other.disconnected_cb;
  initiated_cb = other.initiated_cb;
  rejected_cb = other.rejected_cb;
}

ConnectionListenerW::ConnectionListenerW(ConnectionListenerW &&other) noexcept =
    default;

DiscoveryListenerW::DiscoveryListenerW(
    EndpointFoundCB endpointFoundCB, EndpointLostCB endpointLostCB,
    EndpointDistanceChangedCB endpointDistanceChangedCB)
    : endpoint_found_cb(endpointFoundCB),
      endpoint_lost_cb(endpointLostCB),
      endpoint_distance_changed_cb(endpointDistanceChangedCB),
      impl_(new connections::DiscoveryListener()) {
  CHECK(endpoint_distance_changed_cb != nullptr);
  auto epdc = endpoint_distance_changed_cb;
  impl_->endpoint_distance_changed_cb =
      [epdc](const std::string &endpoint_id,
             connections::DistanceInfo distance_info) {
        DistanceInfoW distanceInfoW = DistanceInfoW::kUnknown;
        switch (distance_info) {
          case connections::DistanceInfo::kFar:
            distanceInfoW = DistanceInfoW::kFar;
            break;
          case connections::DistanceInfo::kClose:
            distanceInfoW = DistanceInfoW::kFar;
            break;
          case connections::DistanceInfo::kVeryClose:
            distanceInfoW = DistanceInfoW::kVeryClose;
            break;
          case connections::DistanceInfo::kUnknown:
            break;
        }
        epdc(endpoint_id.c_str(), distanceInfoW);
      };

  CHECK(endpoint_found_cb != nullptr);
  auto epf = endpoint_found_cb;
  impl_->endpoint_found_cb = [epf](const std::string &endpoint_id,
                                   ByteArray endpoint_info,
                                   const std::string &service_id) {
    epf(endpoint_id.c_str(), endpoint_info.data(), endpoint_info.size(),
        service_id.c_str());
  };

  CHECK(endpoint_lost_cb != nullptr);
  auto epl = endpoint_lost_cb;
  impl_->endpoint_lost_cb = [epl](const std::string &endpoint_id) {
    epl(endpoint_id.c_str());
  };
}

DiscoveryListenerW::DiscoveryListenerW(DiscoveryListenerW &other) {
  endpoint_distance_changed_cb = other.endpoint_distance_changed_cb;
  endpoint_found_cb = other.endpoint_found_cb;
  endpoint_lost_cb = other.endpoint_lost_cb;
  impl_ = std::move(other.impl_);
}

DiscoveryListenerW::DiscoveryListenerW(DiscoveryListenerW &&other) noexcept {
  endpoint_distance_changed_cb = other.endpoint_distance_changed_cb;
  endpoint_found_cb = other.endpoint_found_cb;
  endpoint_lost_cb = other.endpoint_lost_cb;
  impl_ = std::move(other.impl_);
}

PayloadListenerW::PayloadListenerW(PayloadCB payloadCB,
                                   PayloadProgressCB payloadProgressCB)
    : payload_cb(payloadCB),
      payload_progress_cb(payloadProgressCB),
      impl_(std::unique_ptr<connections::PayloadListener,
                            connections::PayloadListenerDeleter>(
          new connections::PayloadListener())) {
  CHECK(payload_cb != nullptr);
  auto pcb = payload_cb;
  impl_->payload_cb = [pcb](absl::string_view endpoint_id,
                            connections::Payload payload) {
    PayloadW payloadW;

    switch (payload.GetType()) {
      case connections::PayloadType::kBytes: {
        payloadW = PayloadW(payload.GetId(), payload.AsBytes().data(),
                            payload.AsBytes().size());
        break;
      }
      case connections::PayloadType::kFile: {
        InputFileW file(std::move(payload.AsFile()));
        payloadW = PayloadW(payload.GetId(), std::move(file));
      } break;

      // TODO(jfcarroll): Figure out how to capture type kStream.
      // case connections::PayloadType::kStream: {
      //  payloadW = PayloadW(payload.AsStream());
      //}
      case connections::PayloadType::kStream: {
        InputFileW file(std::move(payload.AsFile()));
        payloadW = PayloadW(payload.GetId(), std::move(file));
      } break;
      case connections::PayloadType::kUnknown: {
        // Throw exception here?
        break;
      }
    }
    pcb(std::string(endpoint_id).c_str(), payloadW);
  };

  CHECK(payload_progress_cb != nullptr);
  auto ppcb = payload_progress_cb;
  impl_->payload_progress_cb =
      [ppcb](absl::string_view endpoint_id,
             connections::PayloadProgressInfo payload_progress_info) {
        PayloadProgressInfoW payload_progress_info_w;
        payload_progress_info_w.payload_id = payload_progress_info.payload_id;
        payload_progress_info_w.total_bytes = payload_progress_info.total_bytes;
        payload_progress_info_w.bytes_transferred =
            payload_progress_info.bytes_transferred;

        switch (payload_progress_info.status) {
          case connections::PayloadProgressInfo::Status::kCanceled:
            payload_progress_info_w.status =
                PayloadProgressInfoW::Status::kCanceled;
            break;
          case connections::PayloadProgressInfo::Status::kFailure:
            payload_progress_info_w.status =
                PayloadProgressInfoW::Status::kFailure;
            break;
          case connections::PayloadProgressInfo::Status::kInProgress:
            payload_progress_info_w.status =
                PayloadProgressInfoW::Status::kInProgress;
            break;
          case connections::PayloadProgressInfo::Status::kSuccess:
            payload_progress_info_w.status =
                PayloadProgressInfoW::Status::kSuccess;
            break;
        }

        ppcb(std::string(endpoint_id).c_str(), payload_progress_info_w);
      };
}

PayloadListenerW::PayloadListenerW(PayloadListenerW &other) {
  impl_ = std::move(other.impl_);
}

PayloadListenerW::PayloadListenerW(PayloadListenerW &&other) noexcept {
  impl_ = std::move(other.impl_);
}

}  // namespace windows
}  // namespace nearby
