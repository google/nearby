// Copyright 2026 Google LLC
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

#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_SOCKET_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_SOCKET_H_

#include <cstdint>

#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/socket.h"

namespace nearby {
namespace connections {
namespace mediums {

// A base implementation that creates a non-working WebRtcSocket that can be
// used as a placeholder when WebRTC is disabled.
class WebRtcSocket : public Socket {
 public:
  ~WebRtcSocket() override = default;

  InputStream& GetInputStream() override { return fake_input_stream_; }

  OutputStream& GetOutputStream() override { return fake_output_stream_; }

  Exception Close() override { return {Exception::kSuccess}; }

  virtual bool IsValid() const { return false; }

 private:
  class FakeInputStream : public InputStream {
   public:
    ExceptionOr<ByteArray> Read(std::int64_t size) override {
      return {Exception::kSuccess};
    }
    Exception Close() override { return {Exception::kSuccess}; }
  };

  class FakeOutputStream : public OutputStream {
   public:
    Exception Write(absl::string_view data) override {
      return {Exception::kSuccess};
    }
    Exception Flush() override { return {Exception::kSuccess}; }
    Exception Close() override { return {Exception::kSuccess}; }
  };

  FakeInputStream fake_input_stream_;
  FakeOutputStream fake_output_stream_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_SOCKET_H_
