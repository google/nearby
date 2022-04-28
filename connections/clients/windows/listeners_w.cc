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

#include "connections/clients/windows/listeners_w.h"

#include "connections/listeners.h"

namespace location {
namespace nearby {
// Must implement Deleters, since the connections classes weren't
// fully defined in the header
namespace connections {
void ResultCallbackDeleter::operator()(connections::ResultCallback *p) {
  delete p;
}
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
    : impl_(std::unique_ptr<connections::ResultCallback,
                            connections::ResultCallbackDeleter>(
          new connections::ResultCallback())) {
  ResultCallbackImpl = this;
  impl_->result_cb = ResultCB;
}

ResultCallbackW::~ResultCallbackW() {}

ResultCallbackW::ResultCallbackW(ResultCallbackW &other) {
  impl_ = std::move(other.impl_);
}

ResultCallbackW::ResultCallbackW(ResultCallbackW &&other) noexcept {
  impl_ = std::move(other.impl_);
}

ConnectionListenerW::ConnectionListenerW()
    : impl_(std::unique_ptr<connections::ConnectionListener,
                            connections::ConnectionListenerDeleter>(
          new connections::ConnectionListener())) {
  impl_->initiated_cb =
      [this](
          const std::string &endpoint_id,
          const connections::ConnectionResponseInfo connection_response_info) {
        if (this->initiated_cb == nullptr) {
          return;
        }
        ConnectionResponseInfoW connection_response_info_w{
            connection_response_info.remote_endpoint_info.data(),
            connection_response_info.remote_endpoint_info.size(),
            connection_response_info.authentication_token.c_str(),
            connection_response_info.raw_authentication_token.data(),
            connection_response_info.raw_authentication_token.size(),
            connection_response_info.is_incoming_connection,
            connection_response_info.is_connection_verified};
        this->initiated_cb(endpoint_id.c_str(), connection_response_info_w);
      };
  impl_->accepted_cb = [this](const std::string &endpoint_id) {
    if (this->accepted_cb != nullptr) {
      this->accepted_cb(endpoint_id.c_str());
    }
  };
  impl_->rejected_cb = [this](const std::string &endpoint_id, Status status) {
    if (this->rejected_cb != nullptr) {
      this->rejected_cb(endpoint_id.c_str(), status);
    }
  };
  impl_->disconnected_cb = [this](const std::string &endpoint_id) {
    if (this->disconnected_cb != nullptr) {
      this->disconnected_cb(endpoint_id.c_str());
    }
  };
  impl_->bandwidth_changed_cb = [this](const std::string &endpoint_id,
                                       connections::Medium medium) {
    if (this->bandwidth_changed_cb != nullptr) {
      this->bandwidth_changed_cb(endpoint_id.c_str(), medium);
    }
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

DiscoveryListenerW::DiscoveryListenerW()
    : impl_(new connections::DiscoveryListener()) {
  impl_->endpoint_distance_changed_cb =
      [this](const std::string &endpoint_id,
             connections::DistanceInfo distance_info) {
        if (this->endpoint_distance_changed_cb == nullptr) {
          return;
        }
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
        endpoint_distance_changed_cb(endpoint_id.c_str(), distanceInfoW);
      };
  impl_->endpoint_found_cb = [this](const std::string &endpoint_id,
                                    ByteArray endpoint_info,
                                    const std::string &service_id) {
    if (endpoint_found_cb != nullptr) {
      endpoint_found_cb(endpoint_id.c_str(), endpoint_info.data(),
                        endpoint_info.size(), service_id.c_str());
    }
  };
  impl_->endpoint_lost_cb = [this](const std::string &endpoint_id) {
    if (this->endpoint_lost_cb != nullptr) {
      this->endpoint_lost_cb(endpoint_id.c_str());
    }
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

PayloadListenerW::PayloadListenerW()
    : impl_(std::unique_ptr<connections::PayloadListener,
                            connections::PayloadListenerDeleter>(
          new connections::PayloadListener())) {
  impl_->payload_cb = [this](const std::string &endpoint_id,
                             connections::Payload payload) {
    PayloadW payloadW;

    if (payload_cb == nullptr) {
      return;
    }
    switch (payload.GetType()) {
      case connections::PayloadType::kBytes: {
        payloadW = PayloadW(payload.AsBytes().data(), payload.AsBytes().size());
        break;
      }
      case connections::PayloadType::kFile: {
        InputFile file(std::move(*payload.AsFile()));
        payloadW = PayloadW(file);
      } break;
      // TODO(jfcarroll): Figure out how to capture type kStream.
      // case connections::PayloadType::kStream: {
      //  payloadW = PayloadW(payload.AsStream());
      //}
      case connections::PayloadType::kStream: {
        InputFile file(std::move(*payload.AsFile()));
        payloadW = PayloadW(file);
      } break;
      case connections::PayloadType::kUnknown: {
        // Throw exception here?
        break;
      }
    }
    payload_cb(endpoint_id.c_str(), payloadW);
  };
  impl_->payload_progress_cb =
      [this](const std::string &endpoint_id,
             connections::PayloadProgressInfo payload_progress_info) {
        if (payload_progress_cb == nullptr) {
          return;
        }
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

        payload_progress_cb(endpoint_id.c_str(), payload_progress_info_w);
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
}  // namespace location
