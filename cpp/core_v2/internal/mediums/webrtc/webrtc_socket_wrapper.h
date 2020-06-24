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

#ifndef CORE_V2_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_SOCKET_WRAPPER_H_
#define CORE_V2_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_SOCKET_WRAPPER_H_

#include <memory>

#include "core_v2/internal/mediums/webrtc/webrtc_socket.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {

class WebRtcSocketWrapper final {
 public:
  WebRtcSocketWrapper() = default;
  WebRtcSocketWrapper(const WebRtcSocketWrapper&) = default;
  WebRtcSocketWrapper& operator=(const WebRtcSocketWrapper&) = default;
  explicit WebRtcSocketWrapper(std::unique_ptr<WebRtcSocket> socket)
      : impl_(socket.release()) {}
  ~WebRtcSocketWrapper() = default;

  InputStream& GetInputStream() { return impl_->GetInputStream(); }

  OutputStream& GetOutputStream() { return impl_->GetOutputStream(); }

  void NotifyDataChannelMsgReceived(const ByteArray& message) {
    impl_->NotifyDataChannelMsgReceived(message);
  }

  void NotifyDataChannelBufferedAmountChanged() {
    impl_->NotifyDataChannelBufferedAmountChanged();
  }

  void Close() { return impl_->Close(); }

  bool IsValid() const { return impl_ != nullptr; }

  WebRtcSocket& GetImpl() { return *impl_; }

 private:
  std::shared_ptr<WebRtcSocket> impl_;
};

}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_MEDIUMS_WEBRTC_WEBRTC_SOCKET_WRAPPER_H_
