// Copyright 2022 Google LLC
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

#include <cstring>
#include <exception>

#include "internal/platform/implementation/windows/wifi_direct.h"
#include "internal/platform/logging.h"

namespace nearby {
namespace windows {

WifiDirectSocket::WifiDirectSocket(StreamSocket socket) {
  stream_soket_ = socket;
  input_stream_ = SocketInputStream(socket.InputStream());
  output_stream_ = SocketOutputStream(socket.OutputStream());
}

WifiDirectSocket::~WifiDirectSocket() {
  try {
    if (stream_soket_ != nullptr) {
      Close();
    }
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
  }
}

InputStream& WifiDirectSocket::GetInputStream() { return input_stream_; }

OutputStream& WifiDirectSocket::GetOutputStream() { return output_stream_; }

Exception WifiDirectSocket::Close() {
  try {
    if (stream_soket_ != nullptr) {
      stream_soket_.Close();
    }
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

WifiDirectSocket::SocketInputStream::SocketInputStream(
    IInputStream input_stream) {
  input_stream_ = input_stream;
}

ExceptionOr<ByteArray> WifiDirectSocket::SocketInputStream::Read(
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
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

ExceptionOr<size_t> WifiDirectSocket::SocketInputStream::Skip(size_t offset) {
  try {
    Buffer buffer = Buffer(offset);

    auto ibuffer =
        input_stream_.ReadAsync(buffer, offset, InputStreamOptions::None).get();
    return ExceptionOr((size_t)ibuffer.Length());
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

Exception WifiDirectSocket::SocketInputStream::Close() {
  try {
    input_stream_.Close();
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

// SocketOutputStream
WifiDirectSocket::SocketOutputStream::SocketOutputStream(
    IOutputStream output_stream) {
  output_stream_ = output_stream;
}

Exception WifiDirectSocket::SocketOutputStream::Write(const ByteArray& data) {
  try {
    Buffer buffer = Buffer(data.size());
    std::memcpy(buffer.data(), data.data(), data.size());
    buffer.Length(data.size());

    output_stream_.WriteAsync(buffer).get();
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

Exception WifiDirectSocket::SocketOutputStream::Flush() {
  try {
    output_stream_.FlushAsync().get();
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

Exception WifiDirectSocket::SocketOutputStream::Close() {
  try {
    output_stream_.Close();
    return {Exception::kSuccess};
  } catch (std::exception exception) {
    NEARBY_LOGS(ERROR) << __func__ << ": Exception: " << exception.what();
    return {Exception::kIo};
  } catch (const winrt::hresult_error& error) {
    NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
                       << ": " << winrt::to_string(error.message());
    return {Exception::kIo};
  } catch (...) {
    NEARBY_LOGS(ERROR) << __func__ << ": Unknown exeption.";
    return {Exception::kIo};
  }
}

}  // namespace windows
}  // namespace nearby