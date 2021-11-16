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

#import "third_party/nearby/cpp/platform/impl/ios/Source/Internal/GNCCoreConnection.h"

#include "third_party/nearby/cpp/core/core.h"
#include "third_party/nearby/cpp/core/payload.h"
#include "third_party/nearby/cpp/platform/api/input_file.h"
#import "third_party/nearby/cpp/platform/base/exception.h"
#include "third_party/nearby/cpp/platform/base/input_stream.h"
#import "third_party/nearby/cpp/platform/base/payload_id.h"
#import "third_party/nearby/cpp/platform/impl/ios/Source/GNCConnection.h"
#import "third_party/nearby/cpp/platform/impl/ios/Source/GNCPayload.h"
#import "third_party/nearby/cpp/platform/impl/ios/Source/Internal/GNCCore.h"
#import "third_party/nearby/cpp/platform/impl/ios/Source/Platform/utils.h"
#include "third_party/nearby/cpp/platform/public/file.h"

using ::location::nearby::ByteArrayFromNSData;
using ::location::nearby::CppStringFromObjCString;
using ::location::nearby::InputFile;
using ::location::nearby::InputStream;
using ::location::nearby::connections::Payload;
using ::location::nearby::PayloadId;
using ResultListener = ::location::nearby::connections::ResultCallback;

namespace location {
namespace nearby {
namespace connections {

/**
 * This InputStream subclass takes input from an NSInputStream. The update handler is called for
 * each chunk of data sent, giving the client an opportunity to handle cancelation.
 */
class GNCInputStreamFromNSStream : public InputStream {
 public:
  explicit GNCInputStreamFromNSStream(NSInputStream *nsStream) : nsStream_(nsStream) {
    [nsStream scheduleInRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    [nsStream open];
  }

  ~GNCInputStreamFromNSStream() override { Close(); }

  ExceptionOr<ByteArray> Read() { return Read(kMaxChunkSize); }

  ExceptionOr<ByteArray> Read(std::int64_t size) override {
    uint8_t *bytesRead = new uint8_t[size];
    NSUInteger numberOfBytesToRead = [[NSNumber numberWithLongLong:size] unsignedIntegerValue];
    NSInteger numberOfBytesRead = [nsStream_ read:bytesRead maxLength:numberOfBytesToRead];
    if (numberOfBytesRead == 0) {
      // Reached end of stream.
      return ExceptionOr<ByteArray>();
    } else if (numberOfBytesRead < 0) {
      // Stream error.
      return ExceptionOr<ByteArray>{Exception::kIo};
    }
    return ExceptionOr<ByteArray>(ByteArrayFromNSData([NSData dataWithBytes:bytesRead
                                                                     length:numberOfBytesRead]));
  }

  Exception Close() override {
    [nsStream_ close];
    return {Exception::kSuccess};
  }

 private:
  static const size_t kMaxChunkSize = 32 * 1024;
  NSInputStream *nsStream_;
  // dispatch_block_t update_handler_;
};

}  // namespace connections
}  // namespace nearby
}  // namespace location

@implementation GNCPayloadInfo

+ (instancetype)infoWithProgress:(nullable NSProgress *)progress
                      completion:(GNCPayloadResultHandler)completion {
  GNCPayloadInfo *info = [[GNCPayloadInfo alloc] init];
  info.progress = progress;
  info.completion = completion;
  return info;
}

- (void)callCompletion:(GNCPayloadResult)result {
  if (_completion) _completion(result);
  _completion = nil;
}

@end

@implementation GNCCoreConnection

+ (instancetype)connectionWithEndpointId:(GNCEndpointId)endpointId
                                    core:(GNCCore *)core
                          deallocHandler:(dispatch_block_t)deallocHandler {
  GNCCoreConnection *connection = [[GNCCoreConnection alloc] init];
  connection.endpointId = endpointId;
  connection.core = core;
  connection.deallocHandler = deallocHandler;
  connection.payloads = [[NSMutableDictionary alloc] init];
  return connection;
}

- (void)dealloc {
  _core->_core->DisconnectFromEndpoint(CppStringFromObjCString(_endpointId), ResultListener{});
  _deallocHandler();
}

- (NSProgress *)sendBytesPayload:(GNCBytesPayload *)payload
                      completion:(GNCPayloadResultHandler)completion {
  Payload corePayload(ByteArrayFromNSData(payload.bytes));
  NSUInteger length = payload.bytes.length;
  PayloadId payloadId = corePayload.GetId();
  NSProgress *progress = [NSProgress progressWithTotalUnitCount:length];
  __weak __typeof__(self) weakSelf = self;
  progress.cancellationHandler = ^{
    [weakSelf cancelPayloadWithId:payloadId];
  };
  return [self sendPayload:std::move(corePayload)
                      size:length
                  progress:progress
                completion:completion];
}

- (NSProgress *)sendStreamPayload:(GNCStreamPayload *)payload
                       completion:(GNCPayloadResultHandler)completion {
  NSProgress *progress = [NSProgress progressWithTotalUnitCount:-1];

  PayloadId payloadId = Payload::GenerateId();
  Payload corePayload(payloadId, [payload]() -> InputStream & {
    location::nearby::connections::GNCInputStreamFromNSStream *stream =
        new location::nearby::connections::GNCInputStreamFromNSStream(payload.stream);
    return *stream;
  });
  return [self sendPayload:std::move(corePayload) size:-1 progress:progress completion:completion];
}

- (NSProgress *)sendFilePayload:(GNCFilePayload *)payload
                     completion:(GNCPayloadResultHandler)completion {
  NSProgress *progress = [NSProgress progressWithTotalUnitCount:0];

  std::int64_t fileSize = 0;
  NSURL *fileURL = payload.fileURL;
  NSNumber *fileSizeValue = nil;
  BOOL result = [fileURL getResourceValue:&fileSizeValue forKey:NSURLFileSizeKey error:nil];
  if (result == YES) {
    fileSize = fileSizeValue.longValue;
  }
  PayloadId payloadId = Payload::GenerateId();
  // Add the pair of payloadId and fileURL to the map in the GNCCore.
  [_core insertURLToMapWithPayloadID:payloadId urlToSend:fileURL];

  // TODO(jfcarroll): Need someone familiar with iOS to fix this
  std::string path("FIXME, WE NEED A PATH HERE");
  Payload corePayload(payloadId, InputFile(path.c_str()));
  progress.totalUnitCount = fileSize;
  return [self sendPayload:std::move(corePayload)
                      size:fileSize
                  progress:progress
                completion:completion];
}

#pragma mark Private

- (NSProgress *)sendPayload:(Payload)payload
                       size:(uint64_t)size
                   progress:(NSProgress *)progress
                 completion:(GNCPayloadResultHandler)completion {
  _payloads[@(payload.GetId())] = [GNCPayloadInfo infoWithProgress:progress completion:completion];
  _core->_core->SendPayload(std::vector<std::string>(1, CppStringFromObjCString(_endpointId)),
                            std::move(payload), ResultListener{});
  return progress;
}

- (void)cancelPayloadWithId:(PayloadId)payloadId {
  _core->_core->CancelPayload(payloadId, ResultListener{});
}

@end
