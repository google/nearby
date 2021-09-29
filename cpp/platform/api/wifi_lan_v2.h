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

#ifndef PLATFORM_API_WIFI_LAN__V2_H_
#define PLATFORM_API_WIFI_LAN__V2_H_

#include <string>

#include "platform/base/cancellation_flag.h"
#include "platform/base/input_stream.h"
#include "platform/base/listeners.h"
#include "platform/base/nsd_service_info.h"
#include "platform/base/output_stream.h"

namespace location {
namespace nearby {
namespace api {

// Opaque wrapper over a WifiLan service which contains |NsdServiceInfo|.
class WifiLanServiceV2 {
 public:
  virtual ~WifiLanServiceV2() = default;

  // Returns the |NsdServiceInfo| which contains the packed string of
  // |WifiLanServiceInfo| and the endpoint info with named key in a TXTRecord
  // map.
  // The details refer to
  // https://developer.android.com/reference/android/net/nsd/NsdServiceInfo.html.
  virtual NsdServiceInfo GetServiceInfo() const = 0;
};

class WifiLanSocketV2 {
 public:
  virtual ~WifiLanSocketV2() = default;

  // Returns the InputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  virtual InputStream& GetInputStream() = 0;

  // Returns the OutputStream of the WifiLanSocket.
  // On error, returned stream will report Exception::kIo on any operation.
  //
  // The returned object is not owned by the caller, and can be invalidated once
  // the WifiLanSocket object is destroyed.
  virtual OutputStream& GetOutputStream() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;

  // Returns valid WifiLanService pointer if there is a connection, and
  // nullptr otherwise.
  virtual WifiLanServiceV2* GetRemoteService() = 0;
};

class WifiLanServerSocket {
 public:
  virtual ~WifiLanServerSocket() = default;

  // Returns ip address.
  virtual std::string GetIpAddress() = 0;

  // Returns port.
  virtual int GetPort() = 0;

  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  virtual std::unique_ptr<WifiLanSocketV2> Accept() = 0;

  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  virtual Exception Close() = 0;
};

// Container of operations that can be performed over the WifiLan medium.
class WifiLanMediumV2 {
 public:
  virtual ~WifiLanMediumV2() = default;

  // Starts WifiLan advertising.
  //
  // nsd_service_info - NsdServiceInfo data that's advertised through mDNS
  //                    service.
  // On success if the service is now advertising.
  // On error if the service cannot start to advertise or the service type in
  // NsdServiceInfo has been passed previously which StopAdvertising is not
  // been called.
  virtual bool StartAdvertising(const NsdServiceInfo& nsd_service_info) = 0;

  // Stops WifiLan advertising.
  //
  // nsd_service_info - NsdServiceInfo data that's advertised through mDNS
  //                    service.
  // On success if the service stops advertising.
  // On error if the service cannot stop advertising or the service type in
  // NsdServiceInfo cannot be found.
  virtual bool StopAdvertising(const NsdServiceInfo& nsd_service_info) = 0;

  // Callback that is invoked when a discovered service is found or lost.
  struct DiscoveredServiceCallback {
    std::function<void(WifiLanServiceV2& service)> service_discovered_cb =
        DefaultCallback<WifiLanServiceV2&>();
    std::function<void(WifiLanServiceV2& service)> service_lost_cb =
        DefaultCallback<WifiLanServiceV2&>();
  };

  // Starts the discovery of nearby WifiLan services.
  //
  // Returns uuid once the WifiLan discovery has been initiated. The uuid
  // returned is corresponding to callback, if pass the same callback without
  // StopDiscovery in advance, then there will be two different uuids correspond
  // to the same callback instance, which might cause unexpected behavior.
  // Returns uuid if discovery started successfully.
  // Returns 0 if discovery started unsuccessfully.
  virtual int StartDiscovery(DiscoveredServiceCallback callback) = 0;

  // Stops the discovery of nearby WifiLan services.
  //
  // uuid - The one returned from StartDiscovery.
  // On success if uuid is matched to the callback and will be removed from the
  //            list. If list is empty then stops the WifiLan discovery service.
  // On error if the uuid is not existed, then return immediately.
  virtual bool StopDiscovery(int uuid) = 0;

  // Connects to a WifiLan service.
  // On success, returns a new WifiLanSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<WifiLanSocketV2> ConnectToService(
      WifiLanServiceV2& remote_service,
      CancellationFlag* cancellation_flag) = 0;

  // Listens for incoming connection.
  //
  // port - A port number.
  //         0 : use a random port.
  //   1~65536 : open a server socket on that exact port.
  // On success, returns a new WifiLanServerSocket.
  // On error, returns nullptr.
  virtual std::unique_ptr<WifiLanServerSocket> ListenForService(
      int port = 0) = 0;

  virtual WifiLanServiceV2* GetRemoteService(const std::string& ip_address,
                                             int port) = 0;
};

}  // namespace api
}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_API_WIFI_LAN__V2_H_
