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

#ifndef PLATFORM_BASE_SOCKET_H_
#define PLATFORM_BASE_SOCKET_H_

#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {

// A socket is an endpoint for communication between two machines.
//
// https://docs.oracle.com/javase/8/docs/api/java/net/Socket.html
class Socket {
 public:
  virtual ~Socket() = default;

  virtual InputStream& GetInputStream() = 0;
  virtual OutputStream& GetOutputStream() = 0;
  virtual void Close() = 0;
};

}  // namespace nearby

#endif  // PLATFORM_BASE_SOCKET_H_
