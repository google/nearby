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

#import "connections/swift/NearbyCoreAdapter/Sources/Public/NearbyCoreAdapter/GNCPayload.h"

#import <Foundation/Foundation.h>

#include <string>

#include "connections/payload.h"

#import "connections/swift/NearbyCoreAdapter/Sources/CPPInputStream.h"
#import "connections/swift/NearbyCoreAdapter/Sources/GNCInputStream.h"
#import "connections/swift/NearbyCoreAdapter/Sources/GNCPayload+CppConversions.h"

using ::nearby::ByteArray;
using ::nearby::InputFile;
using ::nearby::connections::Payload;

@implementation GNCPayload (CppConversions)

+ (GNCPayload *)fromCpp:(Payload)payload {
  int64_t payloadId = payload.GetId();
  switch (payload.GetType()) {
    case nearby::connections::PayloadType::kBytes: {
      ByteArray bytes = payload.AsBytes();
      NSData *payloadData = [NSData dataWithBytes:bytes.data() length:bytes.size()];
      return [[GNCBytesPayload alloc] initWithData:payloadData identifier:payloadId];
    }
    case nearby::connections::PayloadType::kFile: {
      InputFile *inputFile = payload.AsFile();
      NSString *filePath = @(inputFile->GetFilePath().c_str());
      NSString *parentFolder = @(payload.GetParentFolder().c_str());
      NSString *fileName = @(payload.GetFileName().c_str());
      NSURL *fileURL = [NSURL fileURLWithPath:filePath];
      return [[GNCFilePayload alloc] initWithFileURL:fileURL
                                        parentFolder:parentFolder
                                            fileName:fileName
                                          identifier:payloadId];
    }
    case nearby::connections::PayloadType::kStream: {
      GNCInputStream *stream = [[GNCInputStream alloc] initWithPayload:std::move(payload)];
      return [[GNCStreamPayload alloc] initWithStream:stream identifier:payloadId];
    }
    case nearby::connections::PayloadType::kUnknown:
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
  return Payload(self.identifier, std::make_unique<CPPInputStream>(self.stream));
}

@end

@implementation GNCFilePayload (CppConversions)

- (Payload)toCpp {
  std::string path = [self.fileURL.path cStringUsingEncoding:[NSString defaultCStringEncoding]];
  std::string folder = [self.parentFolder cStringUsingEncoding:[NSString defaultCStringEncoding]];
  std::string name = [self.fileName cStringUsingEncoding:[NSString defaultCStringEncoding]];
  return Payload(self.identifier, folder, name, InputFile(path, self.totalSize.longLongValue));
}

@end
