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

#include "third_party/nearby/windows/listeners_w.h"

#include "core/listeners.h"

namespace location {
namespace nearby {
namespace windows {

ConnectionListenerW::ConnectionListenerW() {
  pImpl = std::make_unique<connections::ConnectionListener>();
  pImpl->initiated_cb =
      [this](
          const std::string endpoint_id,
          const connections::ConnectionResponseInfo connection_response_info) {
        if (this->initiated_cb != nullptr) {
          this->initiated_cb(endpoint_id.c_str(), connection_response_info);
        }
      };
  pImpl->accepted_cb = [this](const std::string endpoint_id) {
    if (this->accepted_cb != nullptr) {
      this->accepted_cb(endpoint_id.c_str());
    }
  };
  pImpl->rejected_cb = [this](const std::string endpoint_id, Status status) {
    if (this->rejected_cb != nullptr) {
      this->rejected_cb(endpoint_id.c_str(), status);
    }
  };
  pImpl->disconnected_cb = [this](const std::string endpoint_id) {
    if (this->disconnected_cb != nullptr) {
      this->disconnected_cb(endpoint_id.c_str());
    }
  };
  pImpl->bandwidth_changed_cb = [this](const std::string endpoint_id,
                                       connections::Medium medium) {
    if (this->bandwidth_changed_cb != nullptr) {
      this->bandwidth_changed_cb(endpoint_id.c_str(), medium);
    }
  };
}

ConnectionListenerW::ConnectionListenerW(ConnectionListenerW &other) {
  pImpl = std::move(other.pImpl);
  accepted_cb = other.accepted_cb;
  bandwidth_changed_cb = other.bandwidth_changed_cb;
  disconnected_cb = other.disconnected_cb;
  initiated_cb = other.initiated_cb;
  rejected_cb = other.rejected_cb;
}

ConnectionListenerW::ConnectionListenerW(ConnectionListenerW &&other) noexcept {
  pImpl = std::move(other.pImpl);
  accepted_cb = std::move(other.accepted_cb);
  bandwidth_changed_cb = std::move(other.bandwidth_changed_cb);
  disconnected_cb = std::move(other.disconnected_cb);
  initiated_cb = std::move(other.initiated_cb);
  rejected_cb = std::move(other.rejected_cb);
}

DiscoveryListenerW::DiscoveryListenerW() {
  pImpl = std::make_unique<connections::DiscoveryListener>();

  pImpl->endpoint_distance_changed_cb =
      [this](const std::string endpoint_id,
             connections::DistanceInfo distance_info) {
        if (this->endpoint_distance_changed_cb != nullptr) {
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
          }
          endpoint_distance_changed_cb(endpoint_id.c_str(), distanceInfoW);
        }
      };
  pImpl->endpoint_found_cb = [this](const std::string endpoint_id,
                                    ByteArray endpoint_info,
                                    const std::string service_id) {
    if (endpoint_found_cb != nullptr) {
      endpoint_found_cb(endpoint_id.c_str(), endpoint_info.data(),
                        endpoint_info.size(), service_id.c_str());
    }
  };
  pImpl->endpoint_lost_cb = [this](const std::string endpoint_id) {
    if (this->endpoint_lost_cb != nullptr) {
      this->endpoint_lost_cb(endpoint_id.c_str());
    }
  };
}

DiscoveryListenerW::DiscoveryListenerW(DiscoveryListenerW &other) {
  pImpl = std::move(other.pImpl);
  endpoint_distance_changed_cb = other.endpoint_distance_changed_cb;
  endpoint_found_cb = other.endpoint_found_cb;
  endpoint_lost_cb = other.endpoint_lost_cb;
}

DiscoveryListenerW::DiscoveryListenerW(DiscoveryListenerW &&other) noexcept {
  endpoint_distance_changed_cb = std::move(endpoint_distance_changed_cb);
  endpoint_found_cb = std::move(endpoint_found_cb);
  endpoint_lost_cb = std::move(endpoint_lost_cb);
  pImpl = std::move(pImpl);
}

PayloadListenerW::PayloadListenerW() {
  pImpl = std::make_unique<connections::PayloadListener>();

  pImpl->payload_cb = [this](const std::string endpoint_id,
                             connections::Payload payload) {
    PayloadW payloadW;

    if (payload_cb != nullptr) {
      switch (payload.GetType()) {
        case connections::PayloadType::kBytes: {
          payloadW =
              PayloadW(payload.AsBytes().data(), payload.AsBytes().size());
          break;
        }
        case connections::PayloadType::kFile: {
          payloadW = PayloadW(std::move(*payload.AsFile()));
          break;
        }
        // TODO(jfcarroll) Figure out how to capture type kStream.
        // case connections::PayloadType::kStream: {
        //  payloadW = PayloadW(payload.AsStream());
        //}
        case connections::PayloadType::kUnknown: {
          // Throw exception here?
        }
      }
      payload_cb(endpoint_id.c_str(), std::move(payloadW));
    }

    pImpl->payload_progress_cb =
        [this](const std::string endpoint_id,
               connections::PayloadProgressInfo payload_progress_info) {
          if (payload_progress_cb != nullptr) {
            payload_progress_cb(endpoint_id.c_str(), payload_progress_info);
          }
        };
  };
}

PayloadListenerW::PayloadListenerW(PayloadListenerW &other) {
  pImpl = std::move(other.pImpl);
}

PayloadListenerW::PayloadListenerW(PayloadListenerW &&other) noexcept {
  pImpl = std::move(other.pImpl);
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
