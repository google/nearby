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

#ifndef CORE_INTERNAL_MEDIUMS_WEBRTC_SOCKET_STUB_H_
#define CORE_INTERNAL_MEDIUMS_WEBRTC_SOCKET_STUB_H_

#include <memory>

#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace connections {
namespace mediums {
class FakeInputStream : public InputStream {
 public:
  ExceptionOr<ByteArray> Read(std::int64_t size) {
    return {Exception::kSuccess};
  }
  Exception Close() { return {Exception::kSuccess}; }
};

class FakeOutputStream : public OutputStream {
 public:
  Exception Write(const ByteArray& data) override {
    return {Exception::kSuccess};
  }
  Exception Flush() override { return {Exception::kSuccess}; }
  Exception Close() override { return {Exception::kSuccess}; }
};

class WebRtcSocketWrapper final {
 public:
  WebRtcSocketWrapper() = default;
  WebRtcSocketWrapper(const WebRtcSocketWrapper&) = default;
  WebRtcSocketWrapper& operator=(const WebRtcSocketWrapper&) = default;
  ~WebRtcSocketWrapper() = default;

  InputStream& GetInputStream() { return fake_input_stream_; }

  OutputStream& GetOutputStream() { return fake_output_stream_; }

  void Close() {}

  bool IsValid() const { return false; }

 private:
  FakeInputStream fake_input_stream_;
  FakeOutputStream fake_output_stream_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_WEBRTC_SOCKET_STUB_H_
