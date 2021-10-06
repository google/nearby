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
#include "third_party/nearby_connections/windows/core_adapter.h"

#include "absl/strings/str_format.h"

#include "core/core.h"

namespace location {
namespace nearby {
namespace connections {

DLL_API Core* InitCore(ServiceControllerRouter* router) {
  return new Core(router);
}

DLL_API void CloseCore(Core* pCore) {
  if (pCore) {
    pCore->StopAllEndpoints(
        {.result_cb =
             std::function<void(location::nearby::connections::Status)>{
                 [](location::nearby::connections::Status) {}}});
    delete pCore;
  }
}

DLL_API void StartAdvertising(Core* pCore, const char* service_id,
                              ConnectionOptions options,
                              ConnectionRequestInfo info,
                              ResultCallback callback) {
  if (pCore) {
    pCore->StartAdvertising(service_id, options, info, callback);
  }
}

DLL_API void StopAdvertising(Core* pCore, ResultCallback callback) {
  if (pCore) {
    pCore->StopAdvertising(callback);
  }
}

DLL_API void StartDiscovery(Core* pCore, const char* service_id,
                            ConnectionOptions options,
                            DiscoveryListener listener,
                            ResultCallback callback) {
  if (pCore) {
    pCore->StartDiscovery(service_id, options, listener, callback);
  }
}

DLL_API void StopDiscovery(Core* pCore, ResultCallback callback) {
  if (pCore) {
    pCore->StopDiscovery(callback);
  }
}

DLL_API void InjectEndpoint(Core* pCore, char* service_id,
                            OutOfBandConnectionMetadata metadata,
                            ResultCallback callback) {
  if (pCore) {
    pCore->InjectEndpoint(service_id, metadata, callback);
  }
}

DLL_API void RequestConnection(Core* pCore, const char* endpoint_id,
                               ConnectionRequestInfo info,
                               ConnectionOptions options,
                               ResultCallback callback) {
  if (pCore) {
    pCore->RequestConnection(endpoint_id, info, options, callback);
  }
}

DLL_API void AcceptConnection(Core* pCore, const char* endpoint_id,
                              PayloadListener listener,
                              ResultCallback callback) {
  if (pCore) {
    pCore->AcceptConnection(endpoint_id, listener, callback);
  }
}

DLL_API void RejectConnection(Core* pCore, const char* endpoint_id,
                              ResultCallback callback) {
  if (pCore) {
    pCore->RejectConnection(endpoint_id, callback);
  }
}

DLL_API void SendPayload(Core* pCore,
                         // todo(jfcarroll) this is being exported, needs to be
                         // refactored to return a plain old c type
                         absl::Span<const std::string> endpoint_ids,
                         Payload payload, ResultCallback callback) {
  if (pCore) {
    pCore->SendPayload(endpoint_ids, std::move(payload), callback);
  }
}

DLL_API void CancelPayload(Core* pCore, std::int64_t payload_id,
                           ResultCallback callback) {
  if (pCore) {
    pCore->CancelPayload(payload_id, callback);
  }
}

DLL_API void DisconnectFromEndpoint(Core* pCore, char* endpoint_id,
                                    ResultCallback callback) {
  if (pCore) {
    pCore->DisconnectFromEndpoint(endpoint_id, callback);
  }
}

DLL_API void StopAllEndpoints(Core* pCore, ResultCallback callback) {
  if (pCore) {
    pCore->StopAllEndpoints(callback);
  }
}

DLL_API void InitiateBandwidthUpgrade(Core* pCore, char* endpoint_id,
                                      ResultCallback callback) {
  if (pCore) {
    pCore->InitiateBandwidthUpgrade(endpoint_id, callback);
  }
}

DLL_API const char* GetLocalEndpointId(Core* pCore) {
  if (pCore) {
    std::string endpoint_id = pCore->GetLocalEndpointId();
    char* result = new char[endpoint_id.length() + 1];
    absl::SNPrintF(result, endpoint_id.length() + 1, "%s", endpoint_id);
    return result;
  }
  return "Null-Core";
}

DLL_API ServiceControllerRouter* InitServiceControllerRouter() {
  return new ServiceControllerRouter();
}

DLL_API void CloseServiceControllerRouter(ServiceControllerRouter* pRouter) {
  if (pRouter) {
    delete pRouter;
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
