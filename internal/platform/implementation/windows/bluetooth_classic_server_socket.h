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

#ifndef PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_SERVER_SOCKET_H_
#define PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_SERVER_SOCKET_H_

#include <Windows.h>

#include <queue>

#include "internal/platform/implementation/bluetooth_classic.h"
#include "internal/platform/implementation/windows/bluetooth_classic_socket.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Devices.Bluetooth.Rfcomm.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Networking.Sockets.h"
#include "internal/platform/implementation/windows/generated/winrt/base.h"

namespace location {
namespace nearby {
namespace windows {

// Supports listening for an incoming network connection using Bluetooth RFCOMM.
// https://docs.microsoft.com/en-us/uwp/api/windows.networking.sockets.streamsocketlistener?view=winrt-20348
using winrt::Windows::Networking::Sockets::StreamSocketListener;

// Provides data for a ConnectionReceived event on a StreamSocketListener
// object.
// https://docs.microsoft.com/en-us/uwp/api/windows.networking.sockets.streamsocketlistenerconnectionreceivedeventargs?view=winrt-20348
using winrt::Windows::Networking::Sockets::
    StreamSocketListenerConnectionReceivedEventArgs;

// Represents an asynchronous action.
// https://docs.microsoft.com/en-us/uwp/api/windows.foundation.iasyncaction?view=winrt-20348
using winrt::Windows::Foundation::IAsyncAction;

// Specifies the quality of service for a StreamSocket object.
// https://docs.microsoft.com/en-us/uwp/api/windows.networking.sockets.socketqualityofservice?view=winrt-20348
using winrt::Windows::Networking::Sockets::SocketQualityOfService;

// Represents an instance of a local RFCOMM service.
// https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.rfcomm.rfcommserviceprovider?view=winrt-20348
using winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceProvider;

// Represents an RFCOMM service ID.
//  https://docs.microsoft.com/en-us/uwp/api/windows.devices.bluetooth.rfcomm.rfcommserviceid?view=winrt-20348
using winrt::Windows::Devices::Bluetooth::Rfcomm::RfcommServiceId;

// Writes data to an output stream.
// https://docs.microsoft.com/en-us/uwp/api/windows.storage.streams.datawriter?view=winrt-20348
using winrt::Windows::Storage::Streams::DataWriter;

// Specifies the type of character encoding for a stream.
// https://docs.microsoft.com/en-us/uwp/api/windows.storage.streams.unicodeencoding?view=winrt-20348
using winrt::Windows::Storage::Streams::UnicodeEncoding;

// Specifies the level of encryption to use on a StreamSocket object.
// https://docs.microsoft.com/en-us/uwp/api/windows.networking.sockets.socketprotectionlevel?view=winrt-22000
using winrt::Windows::Networking::Sockets::SocketProtectionLevel;

// https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html.
class BluetoothServerSocket : public api::BluetoothServerSocket {
 public:
  BluetoothServerSocket(const std::string service_name,
                        const std::string service_uuid);

  ~BluetoothServerSocket() override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#accept()
  //
  // Blocks until either:
  // - at least one incoming connection request is available, or
  // - ServerSocket is closed.
  // On success, returns connected socket, ready to exchange data.
  // Returns nullptr on error.
  // Once error is reported, it is permanent, and ServerSocket has to be closed.
  std::unique_ptr<api::BluetoothSocket> Accept() override;

  // https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#close()
  //
  // Returns Exception::kIo on error, Exception::kSuccess otherwise.
  Exception Close() override;

  Exception StartListening(bool radioDiscoverable);

  void SetScanMode(bool radioDiscoverable) {
    StopAdvertising();
    radio_discoverable_ = radioDiscoverable;
    StartAdvertising();
  }

 private:
  void InitializeServiceSdpAttributes(RfcommServiceProvider rfcommProvider,
                                      std::string service_name);

  Exception StartAdvertising();
  void StopAdvertising();

  // This is used to store sockets in case Accept hasn't been called. Once
  // Accept has been called the socket is popped from the queue and returned to
  // the caller
  std::queue<std::unique_ptr<BluetoothSocket>> bluetooth_sockets_;

  StreamSocketListener stream_socket_listener_;
  winrt::event_token listener_token_;
  CRITICAL_SECTION critical_section_;
  bool closed_ = false;
  bool radio_discoverable_;

  const std::string service_name_;
  const std::string service_uuid_;

  RfcommServiceProvider rfcomm_provider_;
};

}  // namespace windows
}  // namespace nearby
}  // namespace location
#endif  // PLATFORM_IMPL_WINDOWS_BLUETOOTH_CLASSIC_SERVER_SOCKET_H_
