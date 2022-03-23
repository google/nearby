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

#import "internal/platform/implementation/ios/Source/Internal/GNCPayloadListener.h"

#include <string>

#include "connections/core.h"
#include "connections/listeners.h"
#include "connections/payload.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/file.h"
#import "internal/platform/implementation/ios/Source/GNCConnection.h"
#import "internal/platform/implementation/ios/Source/GNCPayload.h"
#import "internal/platform/implementation/ios/Source/Internal/GNCCore.h"
#import "internal/platform/implementation/ios/Source/Internal/GNCCoreConnection.h"
#import "internal/platform/implementation/ios/Source/Internal/GNCPayload+Internal.h"
#include "internal/platform/implementation/ios/Source/Platform/utils.h"
#include "internal/platform/input_stream.h"

NS_ASSUME_NONNULL_BEGIN

namespace location {
namespace nearby {
namespace connections {

void GNCPayloadListener::OnPayload(const std::string &endpoint_id, Payload payload) {
  GNCConnectionHandlers *handlers = handlers_provider_();
  int64_t payloadId = payload.GetId();

  // Note:  The payload must be destroyed by each individual payload type handler below, because in
  // the Stream payload case, it runs an asynchronous read-write loop, which needs the payload
  // and its stream to live until the stream ends.
  NSMutableDictionary<NSNumber *, GNCPayloadInfo *> *payloads = payloads_provider_();

  switch (payload.GetType()) {
    case PayloadType::kBytes: {
      NSData *data = NSDataFromByteArray(payload.AsBytes());  // don't capture C++ object

      // Wait for the payload transfer update to arrive before calling the Bytes payload handler.
      GNCPayloadInfo *info = [GNCPayloadInfo
          infoWithProgress:nil
                completion:^(GNCPayloadResult result) {
                  NSCAssert(result == GNCPayloadResultSuccess, @"Expected success");
                  if (handlers.bytesPayloadHandler) {
                    // Call the Bytes payload handler.
                    dispatch_async(dispatch_get_main_queue(), ^{
                      handlers.bytesPayloadHandler([GNCBytesPayload payloadWithBytes:data
                                                                          identifier:payloadId]);
                    });
                  }
                }];
      payloads[@(payloadId)] = info;
      break;
    }

    case PayloadType::kStream:
      if (handlers.streamPayloadHandler) {
        // Make a pair of bound streams so data pumped into the output stream becomes
        // available for reading from the input stream.
        NSInputStream *clientInputStream;
        NSOutputStream *clientOutputStream;
        // TODO(b/169292092): Base on medium's bandwidth?
        [NSStream getBoundStreamsWithBufferSize:1024
                                    inputStream:&clientInputStream
                                   outputStream:&clientOutputStream];

        NSProgress *progress = [NSProgress progressWithTotalUnitCount:-1];  // indeterminate
        progress.cancellable = YES;

        // Pass the payload to the stream payload handler, receiving the completion handler from it.
        // Since it returns a value, it must be called synchronously.
        __block GNCPayloadResultHandler completion;
        dispatch_sync(dispatch_get_main_queue(), ^{
          completion = handlers.streamPayloadHandler(
              [GNCStreamPayload payloadWithStream:clientInputStream identifier:payloadId],
              progress);
        });
        GNCPayloadInfo *info = [GNCPayloadInfo infoWithProgress:progress completion:completion];
        payloads[@(payloadId)] = info;

        // This is a loop that reads data from the C++ input stream and writes it to the output
        // stream that feeds it to the client input stream.
        __block InputStream *payloadInputStream = payload.AsStream();
        dispatch_queue_t queue =
            dispatch_queue_create("StreamReceiverQueue", DISPATCH_QUEUE_SERIAL);
        dispatch_async(queue, ^{
          [clientOutputStream open];
          while (true) {
            if (progress.isCancelled) {
              // Payload was canceled by the client.
              core_->_core->CancelPayload(payloadId, ResultCallback{.result_cb = [](Status status) {
                                            // TODO(b/148640962): Implement.
                                          }});
              break;
            }

            ExceptionOr<ByteArray> readResult = payloadInputStream->Read(1024);
            if (!readResult.ok()) {
              // Error reading from stream.
              // TODO(b/169292092): Tell core an error has occurred?
              dispatch_async(dispatch_get_main_queue(), ^{
                [info callCompletion:GNCPayloadResultFailure];
              });
              break;
            }
            ByteArray byteArray = readResult.GetResult();
            if (byteArray.Empty()) {
              // End of stream.
              break;
            }

            // Loop until it's all been consumed by the client output stream.
            NSData *data = NSDataFromByteArray(byteArray);
            NSUInteger totalLength = data.length;
            NSUInteger totalNumberWritten = 0;
            while (totalNumberWritten < totalLength) {
              NSInteger numberWritten =
                  [clientOutputStream write:&((const uint8_t *)data.bytes)[totalNumberWritten]
                                  maxLength:totalLength - totalNumberWritten];
              if (numberWritten <= 0) {  // stream error or reached end of stream
                // TODO(b/169292092): Tell core an error has occurred?
                dispatch_async(dispatch_get_main_queue(), ^{
                  [info callCompletion:GNCPayloadResultFailure];
                });
                break;
              }
              totalNumberWritten += numberWritten;
            }
          }
        });
      }
      break;

    case PayloadType::kFile:
      if (handlers.filePayloadHandler) {
        InputFile *payloadInputFile = payload.AsFile();
        NSURL *fileURL =
            [NSURL URLWithString:ObjCStringFromCppString(payloadInputFile->GetFilePath())];
        int64_t fileSize = payloadInputFile->GetTotalSize();
        NSProgress *progress = [NSProgress progressWithTotalUnitCount:fileSize];
        progress.cancellable = YES;
        progress.cancellationHandler = ^{
          // Payload was canceled by the client.
          core_->_core->CancelPayload(payloadId, ResultCallback{.result_cb = [](Status status) {
                                        // TODO(b/148640962): Implement.
                                      }});
        };

        // Pass the payload to the file payload handler, receiving the completion handler from it.
        // Since it returns a value, it must be called synchronously.
        __block GNCPayloadResultHandler completion;
        void (^passPayloadBlock)(void) = ^{
          completion = handlers.filePayloadHandler(
              [GNCFilePayload payloadWithFileURL:fileURL identifier:payloadId], progress);
        };
        if ([NSThread isMainThread]) {
          passPayloadBlock();
        } else {
          dispatch_sync(dispatch_get_main_queue(), passPayloadBlock);
        }
        GNCPayloadInfo *info = [GNCPayloadInfo infoWithProgress:progress completion:completion];
        payloads[@(payloadId)] = info;
      }
      break;

    default:
      ;// fall through
  }
}

void GNCPayloadListener::OnPayloadProgress(const std::string &endpoint_id,
                                           const PayloadProgressInfo &info) {
  // Note: The logic in this callback for handling progress updates and payload completion is
  // identical for Bytes, Stream and File payloads.
  NSMutableDictionary<NSNumber *, GNCPayloadInfo *> *payloads = payloads_provider_();
  NSNumber *payloadId = @(info.payload_id);
  GNCPayloadInfo *payloadInfo = payloads[payloadId];
  if (payloadInfo) {
    // Update the progress.
    if (payloadInfo.progress) {
      payloadInfo.progress.completedUnitCount = info.bytes_transferred;
    }

    // Call the completion handler for success/failure/canceled, but not in-progress.
    if (info.status == PayloadProgressInfo::Status::kInProgress) {
      return;
    }
    GNCPayloadResult result =
        (info.status == PayloadProgressInfo::Status::kSuccess)    ? GNCPayloadResultSuccess
        : (info.status == PayloadProgressInfo::Status::kCanceled) ? GNCPayloadResultCanceled
                                                                  : GNCPayloadResultFailure;
    dispatch_async(dispatch_get_main_queue(), ^{
      payloadInfo.completion(result);
    });

    // Release the payload info.
    [payloads removeObjectForKey:payloadId];
  }
}

}  // namespace connections
}  // namespace nearby
}  // namespace location

NS_ASSUME_NONNULL_END
