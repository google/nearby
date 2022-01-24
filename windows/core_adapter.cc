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
#include "third_party/nearby/windows/core_adapter.h"

#include "absl/strings/str_format.h"
#include "core/core.h"

namespace location {
namespace nearby {
namespace windows {

Core *InitCore(connections::ServiceControllerRouter *router) {
  return new nearby::connections::Core(router);
}

void CloseCore(Core *pCore) {
  if (pCore) {
    pCore->StopAllEndpoints(
        {.result_cb = std::function<void(Status)>{[](Status) {}}});
    delete pCore;
  }
}

void StartAdvertising(Core *pCore, const char *service_id,
                      AdvertisingOptions advertising_options,
                      windows::ConnectionRequestInfoW info,
                      connections::ResultCallback callback) {
  if (pCore) {
    connections::ConnectionRequestInfo crInfo =
        connections::ConnectionRequestInfo();

    crInfo.endpoint_info = ByteArray(info.endpoint_info);
    crInfo.listener = std::move(*(info.listener.GetpImpl()));
    pCore->StartAdvertising(service_id, advertising_options, crInfo, callback);
  }
}

void StopAdvertising(connections::Core *pCore,
                     connections::ResultCallback callback) {
  if (pCore) {
    pCore->StopAdvertising(callback);
  }
}

void StartDiscovery(connections::Core *pCore, const char *service_id,
                    connections::DiscoveryOptions discovery_options,
                    windows::DiscoveryListenerW listener,
                    connections::ResultCallback callback) {
  if (pCore) {
    connections::DiscoveryListener discoveryListener = *listener.GetpImpl();
    pCore->StartDiscovery(service_id, discovery_options, discoveryListener,
                          callback);
  }
}

void StopDiscovery(connections::Core *pCore,
                   connections::ResultCallback callback) {
  if (pCore) {
    pCore->StopDiscovery(callback);
  }
}

void InjectEndpoint(connections::Core *pCore, char *service_id,
                    connections::OutOfBandConnectionMetadata metadata,
                    connections::ResultCallback callback) {
  if (pCore) {
    pCore->InjectEndpoint(service_id, metadata, callback);
  }
}

void RequestConnection(connections::Core *pCore, const char *endpoint_id,
                       windows::ConnectionRequestInfoW info,
                       connections::ConnectionOptions connection_options,
                       connections::ResultCallback callback) {
  if (pCore) {
    connections::ConnectionRequestInfo connectionRequestInfo =
        connections::ConnectionRequestInfo();
    connectionRequestInfo.endpoint_info = ByteArray(info.endpoint_info);
    connectionRequestInfo.listener = std::move(*info.listener.GetpImpl());
    pCore->RequestConnection(endpoint_id, connectionRequestInfo,
                             connection_options, callback);
  }
}

void AcceptConnection(connections::Core *pCore, const char *endpoint_id,
                      windows::PayloadListenerW listener,
                      connections::ResultCallback callback) {
  if (pCore) {
    connections::PayloadListener payload_listener =
        std::move(*listener.GetpImpl());
    pCore->AcceptConnection(endpoint_id, payload_listener, callback);
  }
}

void RejectConnection(connections::Core *pCore, const char *endpoint_id,
                      connections::ResultCallback callback) {
  if (pCore) {
    pCore->RejectConnection(endpoint_id, callback);
  }
}

void SendPayload(connections::Core *pCore,
                 // todo(jfcarroll) this is being exported, needs to be
                 // refactored to return a plain old c type
                 const char *endpoint_ids, windows::PayloadW payloadw,
                 connections::ResultCallback callback) {
  if (pCore) {
    auto payload = payloadw.GetpImpl();
    std::string payloadData = std::string(endpoint_ids);
    absl::Span<const std::string> span{&payloadData, 1};
    pCore->SendPayload(span, std::move(*payload), callback);
  }
}

void CancelPayload(connections::Core *pCore, std::int64_t payload_id,
                   connections::ResultCallback callback) {
  if (pCore) {
    pCore->CancelPayload(payload_id, callback);
  }
}

void DisconnectFromEndpoint(connections::Core *pCore, char *endpoint_id,
                            connections::ResultCallback callback) {
  if (pCore) {
    pCore->DisconnectFromEndpoint(endpoint_id, callback);
  }
}

void StopAllEndpoints(connections::Core *pCore,
                      connections::ResultCallback callback) {
  if (pCore) {
    pCore->StopAllEndpoints(callback);
  }
}

void InitiateBandwidthUpgrade(connections::Core *pCore, char *endpoint_id,
                              connections::ResultCallback callback) {
  if (pCore) {
    pCore->InitiateBandwidthUpgrade(endpoint_id, callback);
  }
}

const char *GetLocalEndpointId(connections::Core *pCore) {
  if (pCore) {
    std::string endpoint_id = pCore->GetLocalEndpointId();
    char *result = new char[endpoint_id.length() + 1];
    absl::SNPrintF(result, endpoint_id.length() + 1, "%s", endpoint_id);
    return result;
  }
  return "Null-Core";
}

connections::ServiceControllerRouter *InitServiceControllerRouter() {
  return new connections::ServiceControllerRouter();
}

void CloseServiceControllerRouter(
    connections::ServiceControllerRouter *pRouter) {
  if (pRouter) {
    delete pRouter;
  }
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
