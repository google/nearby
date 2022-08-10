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

#import "connections/clients/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCPayload.h"

#import <Foundation/Foundation.h>

#include <string>

#include "connections/payload.h"

#import "connections/clients/swift/NearbyCoreAdapter/Sources/CPPInputStreamBinding.h"
#import "connections/clients/swift/NearbyCoreAdapter/Sources/GNCInputStream.h"
#import "connections/clients/swift/NearbyCoreAdapter/Sources/GNCPayload+CppConversions.h"

using ::location::nearby::ByteArray;
using ::location::nearby::InputFile;
using ::location::nearby::InputStream;
using ::location::nearby::connections::Payload;

@implementation GNCPayload (CppConversions)

+ (GNCPayload *)fromCpp:(Payload)payload {
  int64_t payloadId = payload.GetId();
  switch (payload.GetType()) {
    case location::nearby::connections::PayloadType::kBytes: {
      ByteArray bytes = payload.AsBytes();
      NSData *payloadData = [NSData dataWithBytes:bytes.data() length:bytes.size()];
      return [[GNCBytesPayload alloc] initWithData:payloadData identifier:payloadId];
    }
    case location::nearby::connections::PayloadType::kFile: {
      InputFile *inputFile = payload.AsFile();
      NSString *filePath = @(inputFile->GetFilePath().c_str());
      NSURL *fileURL = [NSURL fileURLWithPath:filePath];
      return [[GNCFilePayload alloc] initWithFileURL:fileURL identifier:payloadId];
    }
    case location::nearby::connections::PayloadType::kStream: {
      GNCInputStream *stream = [[GNCInputStream alloc] initWithCppInputStream:payload.AsStream()];
      return [[GNCStreamPayload alloc] initWithStream:stream identifier:payloadId];
    }
    case location::nearby::connections::PayloadType::kUnknown:
      return nil;
  }
}

- (Payload)toCpp {
  return Payload();
}

@end

@implementation GNCBytesPayload (CppConversions)

- (Payload)toCpp {
  return Payload(self.identifier, ByteArray((const char *)self.data.bytes, self.data.length));
}

@end

@implementation GNCStreamPayload (CppConversions)

- (Payload)toCpp {
  // GNCStreamPayload will most likely be destroyed almost immediately, so a weak self would be
  // useless and a strong self will cause a retain cycle. This is why we are keeping a weak
  // reference of the stream instead. The input stream should be kept alive by a strong reference
  // on the user end.
  __weak NSInputStream *stream = self.stream;
  return Payload(self.identifier, [stream]() -> InputStream & {
    return [CPPInputStreamBinding getRefFromStream:stream];
  });
}

@end

@implementation GNCFilePayload (CppConversions)

- (Payload)toCpp {
  NSNumber *fileSize;
  BOOL result = [self.fileURL getResourceValue:&fileSize forKey:NSURLFileSizeKey error:nil];
  if (!result) {
    fileSize = [NSNumber numberWithInt:-1];
  }
  std::string path = [self.fileURL.path cStringUsingEncoding:[NSString defaultCStringEncoding]];
  return Payload(self.identifier, InputFile(path, fileSize.longLongValue));
}

@end
