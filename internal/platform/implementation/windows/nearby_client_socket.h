// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_NEARBY_CLIENT_SOCKET_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_NEARBY_CLIENT_SOCKET_H_

#include <winsock2.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"

namespace nearby::windows {

class NearbyClientSocket {
 public:
  NearbyClientSocket();
  explicit NearbyClientSocket(SOCKET socket);
  ~NearbyClientSocket();

  bool Connect(const std::string& ip_address, int port,
               bool dual_stack = false);
  ExceptionOr<ByteArray> Read(std::int64_t size);
  ExceptionOr<size_t> Skip(size_t offset);
  Exception Write(const ByteArray& data);
  Exception Flush();
  Exception Close();

 private:
  bool is_socket_initiated_ = false;
  SOCKET socket_ = INVALID_SOCKET;
};

}  // namespace nearby::windows

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_IMPLEMENTATION_WINDOWS_NEARBY_CLIENT_SOCKET_H_
