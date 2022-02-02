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

#include "internal/platform/implementation/windows/bluetooth_classic_server_socket.h"

#include <codecvt>
#include <locale>
#include <string>

#include "internal/platform/implementation/windows/bluetooth_classic_socket.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.Collections.h"
#include "internal/platform/implementation/windows/utils.h"
#include "internal/platform/logging.h"

namespace location {
namespace nearby {
namespace windows {

BluetoothServerSocket::BluetoothServerSocket(const std::string service_name,
                                             const std::string service_uuid)
    : radio_discoverable_(false),
      service_name_(service_name),
      service_uuid_(service_uuid),
      rfcomm_provider_(nullptr) {
  InitializeCriticalSection(&critical_section_);
}

BluetoothServerSocket::~BluetoothServerSocket() {}

// https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#accept()
//
// Blocks until either:
// - at least one incoming connection request is available, or
// - ServerSocket is closed.
// On success, returns connected socket, ready to exchange data.
// Returns nullptr on error.
// Once error is reported, it is permanent, and ServerSocket has to be closed.
std::unique_ptr<api::BluetoothSocket> BluetoothServerSocket::Accept() {
  while (bluetooth_sockets_.empty() && !closed_) {
    Sleep(1000);
  }

  EnterCriticalSection(&critical_section_);
  if (!closed_) {
    std::unique_ptr<BluetoothSocket> bluetoothSocket =
        std::move(bluetooth_sockets_.front());
    bluetooth_sockets_.pop();
    LeaveCriticalSection(&critical_section_);

    return std::move(bluetoothSocket);
  } else {
    bluetooth_sockets_ = {};
    LeaveCriticalSection(&critical_section_);
  }

  return nullptr;
}

Exception BluetoothServerSocket::StartListening(bool radioDiscoverable) {
  EnterCriticalSection(&critical_section_);

  radio_discoverable_ = radioDiscoverable;

  // Create the StreamSocketListener
  stream_socket_listener_ = StreamSocketListener();

  // Configure control property
  stream_socket_listener_.Control().QualityOfService(
      SocketQualityOfService::LowLatency);

  stream_socket_listener_.Control().KeepAlive(true);

  // Note From the perspective of a StreamSocket, a Parallel Patterns Library
  // (PPL) completion handler is done executing (and the socket is eligible for
  // disposal) before the continuation body runs. So, to keep your socket from
  // being disposed if you want to use it inside a continuation, you'll need to
  // use one of the techniques described in References to StreamSockets in C++
  // PPL continuations.
  // Assign ConnectionReceived event to event handler on the server socket
  stream_socket_listener_.ConnectionReceived(
      [this](StreamSocketListener streamSocketListener,
             StreamSocketListenerConnectionReceivedEventArgs args) {
        EnterCriticalSection(&critical_section_);
        if (!closed_) {
          this->bluetooth_sockets_.push(
              std::make_unique<BluetoothSocket>(args.Socket()));
        }
        LeaveCriticalSection(&critical_section_);
      });

  try {
    auto rfcommProviderRef =
        RfcommServiceProvider::CreateAsync(
            RfcommServiceId::FromUuid(winrt::guid(service_uuid_)))
            .get();

    rfcomm_provider_ = rfcommProviderRef;

    stream_socket_listener_
        .BindServiceNameAsync(
            winrt::to_hstring(rfcomm_provider_.ServiceId().AsString()),
            SocketProtectionLevel::PlainSocket)
        .get();

    // Set the SDP attributes and start Bluetooth advertising
    InitializeServiceSdpAttributes(rfcomm_provider_, service_name_);
  } catch (std::exception exception) {
    // We will log and eat the exception since the caller
    // expects nullptr if it fails
    NEARBY_LOGS(ERROR) << __func__ << ": Exception setting up for listen: "
                       << exception.what();

    LeaveCriticalSection(&critical_section_);

    return {Exception::kFailed};
  }

  StartAdvertising();

  LeaveCriticalSection(&critical_section_);

  return {Exception::kSuccess};
}

Exception BluetoothServerSocket::StartAdvertising() {
  try {
    rfcomm_provider_.StartAdvertising(stream_socket_listener_,
                                      radio_discoverable_);
  } catch (std::exception exception) {
    // We will log and eat the exception since the caller
    // expects nullptr if it fails
    NEARBY_LOGS(ERROR) << __func__ << ": Exception calling StartAdvertising: "
                       << exception.what();

    LeaveCriticalSection(&critical_section_);

    return {Exception::kFailed};
  }

  return {Exception::kSuccess};
}

void BluetoothServerSocket::StopAdvertising() {
  rfcomm_provider_.StopAdvertising();
}

void BluetoothServerSocket::InitializeServiceSdpAttributes(
    RfcommServiceProvider rfcommProvider, std::string service_name) {
  auto sdpWriter = DataWriter();

  // Write the Service Name Attribute.
  sdpWriter.WriteByte(Constants::SdpServiceNameAttributeType);

  // The length of the UTF-8 encoded Service Name SDP Attribute.
  sdpWriter.WriteByte(service_name.size());

  // The UTF-8 encoded Service Name value.
  sdpWriter.UnicodeEncoding(UnicodeEncoding::Utf8);
  sdpWriter.WriteString(winrt::to_hstring(service_name));

  // Set the SDP Attribute on the RFCOMM Service Provider.
  rfcommProvider.SdpRawAttributes().Insert(Constants::SdpServiceNameAttributeId,
                                           sdpWriter.DetachBuffer());
}

// https://developer.android.com/reference/android/bluetooth/BluetoothServerSocket.html#close()
//
// Returns Exception::kIo on error, Exception::kSuccess otherwise.
Exception BluetoothServerSocket::Close() {
  EnterCriticalSection(&critical_section_);
  if (closed_) {
    LeaveCriticalSection(&critical_section_);
    return {Exception::kSuccess};
  }

  rfcomm_provider_.StopAdvertising();
  closed_ = true;
  bluetooth_sockets_ = {};
  LeaveCriticalSection(&critical_section_);

  return {Exception::kSuccess};
}
}  // namespace windows
}  // namespace nearby
}  // namespace location
