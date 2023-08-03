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

#ifndef PLATFORM_PUBLIC_PIPE_H_
#define PLATFORM_PUBLIC_PIPE_H_

#include <memory>
#include <utility>

#include "internal/platform/input_stream.h"
#include "internal/platform/output_stream.h"

namespace nearby {

// Creates a pipe for streaming data between threads.
// ```
//  auto [input, output] = CreatePipe();
//  ReaderThread(std::move(input));
//  WriterThread(std::move(output));
//  ```
//  Pipe stays valid as long as either `input` or `output` exist.
std::pair<std::unique_ptr<InputStream>, std::unique_ptr<OutputStream>>
CreatePipe();

}  // namespace nearby

#endif  // PLATFORM_PUBLIC_PIPE_H_
