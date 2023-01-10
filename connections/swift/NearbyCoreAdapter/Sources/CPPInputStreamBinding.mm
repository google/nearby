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

#import "connections/swift/NearbyCoreAdapter/Sources/CPPInputStreamBinding.h"

#import <Foundation/Foundation.h>
#import <objc/runtime.h>

#include <memory>
#include <vector>

// TODO(b/239758418): Change this to the non-internal version when available.
#include "internal/platform/input_stream.h"

using ::nearby::ByteArray;
using ::nearby::Exception;
using ::nearby::ExceptionOr;
using ::nearby::InputStream;

class CPPInputStream : public InputStream {
 public:
  explicit CPPInputStream(NSInputStream *iStream) : iStream_(iStream) { [iStream_ open]; }

  ~CPPInputStream() override { Close(); }

  ExceptionOr<ByteArray> Read(std::int64_t size) override {
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

  Exception Close() override {
    [iStream_ close];
    return Exception{Exception::kSuccess};
  }

 private:
  // Prevent a retain cycle since NSInputStream will have a strong reference to this object.
  __weak NSInputStream *iStream_;
};

// Wrap a c++ InputStream subclass in objective-c so it can be associated with the original
// NSInputStream object. The association makes it possible for the unique_ptr to live as long as
// the original user provided NSInputStream.
@implementation CPPInputStreamBinding {
 @public
  std::unique_ptr<CPPInputStream> _cppStream;
}

// This field's address is used as a unique identifier.
static char gAssociatedStreamKey;

- (instancetype)initWithStream:(NSInputStream *)stream {
  self = [super init];
  if (self) {
    _cppStream = std::make_unique<CPPInputStream>(stream);
  }
  return self;
}

+ (void)bindToStream:(NSInputStream *)stream {
  CPPInputStreamBinding *binding = [[CPPInputStreamBinding alloc] initWithStream:stream];
  objc_setAssociatedObject(stream, &gAssociatedStreamKey, binding,
                           OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

+ (InputStream &)getRefFromStream:(NSInputStream *)stream {
  CPPInputStreamBinding *binding = objc_getAssociatedObject(stream, &gAssociatedStreamKey);
  return *binding->_cppStream;
}

@end
