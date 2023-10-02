// Copyright 2021-2023 Google LLC
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

#include <windows.h>

#include <chrono>  // NOLINT(build/c++11)
#include <cstdint>
#include <cstring>
#include <exception>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/windows/generated/winrt/Windows.Foundation.h"
#include "internal/platform/implementation/windows/wifi_lan.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/logging.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace windows {
namespace {
using ::winrt::Windows::Foundation::TimeSpan;

constexpr int kWriteTimeoutInSeconds = 10;
}  // namespace

WifiLanSocket::WifiLanSocket(StreamSocket socket) {
  stream_soket_ = socket;
  input_stream_ = SocketInputStream(socket.InputStream());
  output_stream_ = SocketOutputStream(socket.OutputStream());
}

WifiLanSocket::~WifiLanSocket() {
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

InputStream& WifiLanSocket::GetInputStream() { return input_stream_; }

OutputStream& WifiLanSocket::GetOutputStream() { return output_stream_; }

Exception WifiLanSocket::Close() {
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
      NEARBY_LOGS(WARNING) << "Only read partial of data: [" << ibuffer.Length()
                           << "/" << size << "].";
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

ExceptionOr<size_t> WifiLanSocket::SocketInputStream::Skip(size_t offset) {
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

Exception WifiLanSocket::SocketInputStream::Close() {
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
WifiLanSocket::SocketOutputStream::SocketOutputStream(
    IOutputStream output_stream) {
  output_stream_ = output_stream;
}

Exception WifiLanSocket::SocketOutputStream::Write(const ByteArray& data) {
  try {
    Buffer buffer = Buffer(data.size());
    std::memcpy(buffer.data(), data.data(), data.size());
    buffer.Length(data.size());
    uint32_t wrote_bytes = 0;
    auto write_async = output_stream_.WriteAsync(buffer);

    switch (write_async.wait_for(
        TimeSpan(std::chrono::seconds(kWriteTimeoutInSeconds)))) {
      case winrt::Windows::Foundation::AsyncStatus::Completed:
        wrote_bytes = write_async.GetResults();
        break;
      case winrt::Windows::Foundation::AsyncStatus::Started:
        NEARBY_LOGS(ERROR) << __func__
                           << ": Failed to write socket data due to timeout.";
        write_async.Cancel();
        return {Exception::kIo};
      default:
        NEARBY_LOGS(ERROR)
            << __func__
            << ": Failed to write socket data due to unknown reasons.";
        return {Exception::kIo};
    }

    if (wrote_bytes != data.size()) {
      NEARBY_LOGS(WARNING) << "Only wrote partial of data:[" << wrote_bytes
                           << "/" << data.size() << "].";
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

Exception WifiLanSocket::SocketOutputStream::Flush() {
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

Exception WifiLanSocket::SocketOutputStream::Close() {
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
