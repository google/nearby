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

#include "connections/implementation/offline_frames.h"
#include "internal/platform/byte_array.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  nearby::ByteArray byte_array;
  byte_array.SetData(reinterpret_cast<const char*>(data), size);

  nearby::connections::parser::FromBytes(byte_array);

  return 0;
}
