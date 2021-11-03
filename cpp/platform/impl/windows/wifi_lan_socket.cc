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

#include "platform/impl/windows/wifi_lan.h"
#include "platform/public/logging.h"

namespace location {
namespace nearby {
namespace windows {

WifiLanSocket::WifiLanSocket(StreamSocket socket) {
  stream_soket_ = socket;
  input_stream_ = SocketInputStream(socket.InputStream());
  output_stream_ = SocketOutputStream(socket.OutputStream());
}

WifiLanSocket::~WifiLanSocket() {
  if (stream_soket_ != nullptr) {
    try {
      Close();
    } catch (...) {
      NEARBY_LOGS(ERROR) << "Failed to destructor class WifiLanSocket.";
    }
  }
}

InputStream& WifiLanSocket::GetInputStream() { return input_stream_; }

OutputStream& WifiLanSocket::GetOutputStream() { return output_stream_; }

Exception WifiLanSocket::Close() {
  try {
    if (stream_soket_ != nullptr) {
      stream_soket_.Close();
    }
    return {Exception::kSuccess};
  } catch (...) {
    return {Exception::kIo};
  }
}

// SocketInputStream
WifiLanSocket::SocketInputStream::SocketInputStream(IInputStream input_stream) {
  input_stream_ = input_stream;
}

ExceptionOr<ByteArray> WifiLanSocket::SocketInputStream::Read(
    std::int64_t size) {
  try {
    Buffer buffer = Buffer(size);

    auto ibuffer =
        input_stream_.ReadAsync(buffer, size, InputStreamOptions::None).get();

    if (ibuffer.Length() != size) {
      NEARBY_LOGS(WARNING) << "Only got part of data of needed.";
    }

    ByteArray data((char*)ibuffer.data(), ibuffer.Length());

    return ExceptionOr(data);
  } catch (...) {
    return Exception{Exception::kIo};
  }
}

ExceptionOr<size_t> WifiLanSocket::SocketInputStream::Skip(size_t offset) {
  try {
    Buffer buffer = Buffer(offset);

    auto ibuffer =
        input_stream_.ReadAsync(buffer, offset, InputStreamOptions::None).get();
    return ExceptionOr((size_t)ibuffer.Length());
  } catch (...) {
    return Exception{Exception::kIo};
  }
}

Exception WifiLanSocket::SocketInputStream::Close() {
  try {
    input_stream_.Close();
  } catch (...) {
    return {Exception::kIo};
  }

  return {Exception::kSuccess};
}

// SocketOutputStream
WifiLanSocket::SocketOutputStream::SocketOutputStream(
    IOutputStream output_stream) {
  output_stream_ = output_stream;
}

Exception WifiLanSocket::SocketOutputStream::Write(const ByteArray& data) {
  Buffer buffer = Buffer(data.size());
  std::memcpy(buffer.data(), data.data(), data.size());
  buffer.Length(data.size());

  try {
    output_stream_.WriteAsync(buffer).get();
  } catch (...) {
    return {Exception::kIo};
  }

  return {Exception::kSuccess};
}

Exception WifiLanSocket::SocketOutputStream::Flush() {
  try {
    output_stream_.FlushAsync().get();
  } catch (...) {
    return {Exception::kIo};
  }

  return {Exception::kSuccess};
}

Exception WifiLanSocket::SocketOutputStream::Close() {
  try {
    output_stream_.Close();
  } catch (...) {
    return {Exception::kIo};
  }

  return {Exception::kSuccess};
}

}  // namespace windows
}  // namespace nearby
}  // namespace location
