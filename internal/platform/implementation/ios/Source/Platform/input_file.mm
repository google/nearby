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

#import "internal/platform/implementation/ios/Source/Platform/input_file.h"

#include <string>

#import "internal/platform/exception.h"
#import "internal/platform/implementation/ios/Source/Platform/utils.h"

namespace location {
namespace nearby {
namespace ios {

InputFile::InputFile(NSURL *nsURL) : nsURL_(nsURL) {
  std::string string = CppStringFromObjCString([nsURL_ absoluteString]);
  nsStream_ = [NSInputStream inputStreamWithURL:nsURL_];
  [nsStream_ scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  [nsStream_ open];
}

InputFile::InputFile(absl::string_view file_path, std::int64_t size)
    : path_(file_path), total_size_(size) {
  // TODO(jfcarroll): This is not implemented for iOS yet.
}

ExceptionOr<ByteArray> InputFile::Read(std::int64_t size) {
  uint8_t *bytes_read = new uint8_t[size];
  NSUInteger numberOfBytesToRead = [[NSNumber numberWithLongLong:size] unsignedIntegerValue];
  NSInteger numberOfBytesRead = [nsStream_ read:bytes_read maxLength:numberOfBytesToRead];
  if (numberOfBytesRead == 0) {
    // Reached end of stream.
    return ExceptionOr<ByteArray>();
  } else if (numberOfBytesRead < 0) {
    // Stream error.
    return ExceptionOr<ByteArray>(Exception::kIo);
  }
  return ExceptionOr<ByteArray>(ByteArrayFromNSData([NSData dataWithBytes:bytes_read
                                                                   length:numberOfBytesRead]));
}

std::string InputFile::GetFilePath() const {
  return CppStringFromObjCString([nsURL_ absoluteString]);
}

std::int64_t InputFile::GetTotalSize() const {
  NSNumber *fileSizeValue = nil;
  BOOL result = [nsURL_ getResourceValue:&fileSizeValue forKey:NSURLFileSizeKey error:nil];
  if (result) {
    return fileSizeValue.longValue;
  } else {
    return 0;
  }
}

Exception InputFile::Close() {
  [nsStream_ close];
  return {Exception::kSuccess};
}

}  // namespace ios
}  // namespace nearby
}  // namespace location
