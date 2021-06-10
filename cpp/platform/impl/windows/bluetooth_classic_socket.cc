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

#include "platform/impl/windows/bluetooth_classic_socket.h"

namespace location::nearby::windows {
BluetoothSocket::~BluetoothSocket() {}

// NOTE:
// It is an undefined behavior if GetInputStream() or GetOutputStream() is
// called for a not-connected BluetoothSocket, i.e. any object that is not
// returned by BluetoothClassicMedium::ConnectToService() for client side or
// BluetoothServerSocket::Accept() for server side of connection.

// Returns the InputStream of this connected BluetoothSocket.
// TODO(b/184975123): replace with real implementation.
InputStream& BluetoothSocket::GetInputStream() { return BluetoothSocket::fake_input_stream_; }

// Returns the OutputStream of this connected BluetoothSocket.
// TODO(b/184975123): replace with real implementation.
OutputStream& BluetoothSocket::GetOutputStream() {
  return BluetoothSocket::fake_output_stream_;
}

// Closes both input and output streams, marks Socket as closed.
// After this call object should be treated as not connected.
// Returns Exception::kIo on error, Exception::kSuccess otherwise.
// TODO(b/184975123): replace with real implementation.
Exception BluetoothSocket::Close() { return Exception(); }

// https://developer.android.com/reference/android/bluetooth/BluetoothSocket.html#getRemoteDevice()
// Returns valid BluetoothDevice pointer if there is a connection, and
// nullptr otherwise.
// TODO(b/184975123): replace with real implementation.
api::BluetoothDevice* BluetoothSocket::GetRemoteDevice() { return nullptr; }
}  // namespace location::nearby::windows