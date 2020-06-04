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

#ifndef PLATFORM_API_WIFI_LAN_H_
#define PLATFORM_API_WIFI_LAN_H_

#include "platform/api/input_stream.h"
#include "platform/api/output_stream.h"
#include "platform/byte_array.h"
#include "platform/exception.h"
#include "platform/port/string.h"
#include "platform/ptr.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {

// Opaque wrapper over a WifiLan service which contains encoded service name.
class WifiLanService {
 public:
  virtual ~WifiLanService() = default;

  virtual std::string GetName() = 0;
};

class WifiLanSocket {
 public:
  virtual ~WifiLanSocket() = default;

  // Returns the InputStream of the WifiLanSocket, or a null Ptr<InputStream>
  // on error.
  //
  // The returned Ptr is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  virtual Ptr<InputStream> GetInputStream() = 0;

  // Returns the OutputStream of the WifiLanSocket, or a null
  // Ptr<OutputStream> on error.
  //
  // The returned Ptr is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  virtual Ptr<OutputStream> GetOutputStream() = 0;

  // Returns Exception::IO on error, Exception::NONE otherwise.
  virtual Exception::Value Close() = 0;

  // The returned Ptr is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  virtual Ptr<WifiLanService> GetRemoteWifiLanService() = 0;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiLanMedium {
 public:
  virtual ~WifiLanMedium() = default;

  virtual bool StartAdvertising(
      absl::string_view service_id,
      absl::string_view wifi_lan_service_info_name) = 0;
  virtual void StopAdvertising(absl::string_view service_id) = 0;

  // Callback for WifiLan discover results.
  class DiscoveredServiceCallback {
   public:
    virtual ~DiscoveredServiceCallback() = default;

    virtual void OnServiceDiscovered(Ptr<WifiLanService> wifi_lan_service) = 0;
    virtual void OnServiceLost(Ptr<WifiLanService> wifi_lan_service) = 0;
  };

  virtual bool StartDiscovery(
      absl::string_view service_id,
      Ptr<DiscoveredServiceCallback> discovered_service_callback) = 0;
  virtual void StopDiscovery(absl::string_view service_id) = 0;

  class AcceptedConnectionCallback {
   public:
    virtual ~AcceptedConnectionCallback() = default;

    // The Ptr provided in this callback method will be owned (and
    // destroyed) by the recipient of the callback methods (i.e. the creator of
    // the concrete AcceptedConnectionCallback object).
    virtual void OnConnectionAccepted(Ptr<WifiLanSocket> socket,
                                      absl::string_view service_id) = 0;
  };

  virtual bool StartAcceptingConnections(
      absl::string_view service_id,
      Ptr<AcceptedConnectionCallback> accepted_connection_callback) = 0;
  virtual void StopAcceptingConnections(absl::string_view service_id) = 0;

  virtual Ptr<WifiLanSocket> Connect(Ptr<WifiLanService> wifi_lan_service,
                                     absl::string_view service_id) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_WIFI_LAN_H_
