// Copyright 2023 Google LLC
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

#ifndef PLATFORM_IMPL_LINUX_WIFI_HOTSPOT_SOCKET_H_
#define PLATFORM_IMPL_LINUX_WIFI_HOTSPOT_SOCKET_H_

#include "internal/platform/implementation/linux/stream.h"
#include "internal/platform/implementation/wifi_hotspot.h"

namespace nearby {
namespace linux {
class WifiHotspotSocket : public api::WifiHotspotSocket {
 public:
  explicit WifiHotspotSocket(int connection_fd)
      : fd_(sdbus::UnixFd(connection_fd)),
        output_stream_(fd_),
        input_stream_(fd_) {}

  nearby::InputStream &GetInputStream() override { return input_stream_; };
  nearby::OutputStream &GetOutputStream() override { return output_stream_; };
  Exception Close() override {
    input_stream_.Close();
    output_stream_.Close();

    return Exception{Exception::kSuccess};
  };

 private:
  sdbus::UnixFd fd_;
  OutputStream output_stream_;
  InputStream input_stream_;
};
}  // namespace linux
}  // namespace nearby

#endif
