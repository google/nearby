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
#ifndef THIRD_PARTY_NEARBY_CONNECTIONS_C_INPUT_STREAM_W_H_
#define THIRD_PARTY_NEARBY_CONNECTIONS_C_INPUT_STREAM_W_H_

#include <memory>

namespace nearby {

class InputStream;
struct InputStreamDeleter {
  void operator()(InputStream* p);
};
}  // namespace nearby

namespace nearby {
namespace windows {

class InputStreamW {
 public:
  char* Read(size_t size);

  // throws Exception::kIo
  int64_t Skip(size_t offset);

  // throws Exception::kIo
  int64_t Close();

 private:
  std::unique_ptr<nearby::InputStream, nearby::InputStreamDeleter> impl_;
};

}  // namespace windows
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_CONNECTIONS_C_INPUT_STREAM_W_H_
