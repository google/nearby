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

#import "connections/swift/NearbyCoreAdapter/Sources/CPPInputStream.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <vector>

using ::nearby::ByteArray;
using ::nearby::Exception;
using ::nearby::ExceptionOr;

CPPInputStream::CPPInputStream(NSInputStream *iStream) : iStream_(iStream) { [iStream_ open]; }

CPPInputStream::~CPPInputStream() { Close(); }

ExceptionOr<ByteArray> CPPInputStream::Read(std::int64_t size) {
  std::vector<uint8_t> buffer;
  buffer.reserve(size);
  NSInteger numberOfBytesRead = [iStream_ read:buffer.data() maxLength:size];
  if (numberOfBytesRead == 0) {
    return ExceptionOr<ByteArray>();
  }
  if (numberOfBytesRead < 0) {
    return ExceptionOr<ByteArray>(Exception::kIo);
  }
  return ExceptionOr<ByteArray>(ByteArray((const char *)buffer.data(), numberOfBytesRead));
}

Exception CPPInputStream::Close() {
  [iStream_ close];
  return Exception{Exception::kSuccess};
}
