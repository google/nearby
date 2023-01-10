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
#include "connections/c/input_stream_w.h"

#include "internal/platform/input_stream.h"

namespace nearby {
void InputStreamDeleter::operator()(nearby::InputStream* p) { delete p; }
}  // namespace nearby

namespace nearby {
namespace windows {

char* InputStreamW::Read(size_t size) {
  auto result = impl_->Read(size);
  if (result.ok()) {
    return result.GetResult().data();
  }
  return nullptr;
}

int64_t InputStreamW::Skip(size_t offset) {
  auto result = impl_->Skip(offset);
  if (result.ok()) {
    return result.GetResult();
  }
  return -1;
}

int64_t InputStreamW::Close() {
  auto result = impl_->Close();
  if (result.Ok()) {
    return 0;
  }
  return -1;
}

}  // namespace windows
}  // namespace nearby
