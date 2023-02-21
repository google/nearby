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

#include "internal/platform/implementation/platform.h"

#include <string>
#include <memory>

#include "internal/platform/implementation/apple/atomic_boolean.h"
#include "internal/platform/implementation/apple/atomic_uint32.h"
#include "internal/platform/implementation/apple/ble.h"
#include "internal/platform/implementation/apple/condition_variable.h"
#include "internal/platform/implementation/apple/count_down_latch.h"
#include "internal/platform/implementation/apple/device_info.h"
#import "internal/platform/implementation/apple/log_message.h"
#import "internal/platform/implementation/apple/multi_thread_executor.h"
#include "internal/platform/implementation/apple/mutex.h"
#import "internal/platform/implementation/apple/scheduled_executor.h"
#import "internal/platform/implementation/apple/single_thread_executor.h"
#include "internal/platform/implementation/apple/timer.h"
#import "internal/platform/implementation/apple/utils.h"
#include "internal/platform/implementation/apple/wifi_lan.h"
#include "internal/platform/implementation/mutex.h"
#include "internal/platform/implementation/shared/file.h"
#include "internal/platform/payload_id.h"

namespace nearby {
namespace api {

std::string ImplementationPlatform::GetCustomSavePath(const std::string& parent_folder,
                                                      const std::string& file_name) {
  // TODO(b/227535777): This needs to be done correctly, we now have a file name and parent folder,
  // they should be combined with the custom save path
  NSString* fileName = ObjCStringFromCppString(file_name);

  // TODO(b/227535777): If file name matches an existing file, it will be overwritten. Append a
  // number until a unique file name is reached 'foobar (2).png'.

  return CppStringFromObjCString([NSTemporaryDirectory() stringByAppendingPathComponent:fileName]);
}

std::string ImplementationPlatform::GetDownloadPath(const std::string& parent_folder,
                                                    const std::string& file_name) {
  // TODO(jfcarroll): This needs to be done correctly, we now have a file name and parent folder,
  // they should be combined with the default download path
  NSString* fileName = ObjCStringFromCppString(file_name);

  // TODO(b/227535777): If file name matches an existing file, it will be overwritten. Append a
  // number until a unique file name is reached 'foobar (2).png'.

  return CppStringFromObjCString([NSTemporaryDirectory() stringByAppendingPathComponent:fileName]);
}

OSName ImplementationPlatform::GetCurrentOS() { return OSName::kApple; }

// Atomics:
std::unique_ptr<AtomicBoolean> ImplementationPlatform::CreateAtomicBoolean(bool initial_value) {
  return std::make_unique<apple::AtomicBoolean>(initial_value);
}

std::unique_ptr<AtomicUint32> ImplementationPlatform::CreateAtomicUint32(
    std::uint32_t initial_value) {
  return std::make_unique<apple::AtomicUint32>(initial_value);
}

std::unique_ptr<CountDownLatch> ImplementationPlatform::CreateCountDownLatch(std::int32_t count) {
  return std::make_unique<apple::CountDownLatch>(count);
}

std::unique_ptr<Mutex> ImplementationPlatform::CreateMutex(Mutex::Mode mode) {
  // iOS does not support unchecked Mutex in debug mode, therefore
  // apple::Mutex is used for both kRegular and kRegularNoCheck.
  if (mode == Mutex::Mode::kRecursive) {
    return std::make_unique<apple::RecursiveMutex>();
  } else {
    return std::make_unique<apple::Mutex>();
  }
}

std::unique_ptr<ConditionVariable> ImplementationPlatform::CreateConditionVariable(Mutex* mutex) {
  return std::make_unique<apple::ConditionVariable>(static_cast<apple::Mutex*>(mutex));
}

ABSL_DEPRECATED("This interface will be deleted in the near future.")
std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(PayloadId payload_id,
                                                                   std::int64_t total_size) {
  return nullptr;
}

std::unique_ptr<InputFile> ImplementationPlatform::CreateInputFile(const std::string& file_path,
                                                                   size_t size) {
  return shared::IOFile::CreateInputFile(file_path, size);
}

ABSL_DEPRECATED("This interface will be deleted in the near future.")
std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(PayloadId payload_id) {
  return nullptr;
}

std::unique_ptr<OutputFile> ImplementationPlatform::CreateOutputFile(const std::string& file_path) {
  return shared::IOFile::CreateOutputFile(file_path);
}

std::unique_ptr<LogMessage> ImplementationPlatform::CreateLogMessage(
    const char* file, int line, LogMessage::Severity severity) {
  return std::make_unique<apple::LogMessage>(file, line, severity);
}

// Java-like Executors
std::unique_ptr<SubmittableExecutor> ImplementationPlatform::CreateSingleThreadExecutor() {
  return std::make_unique<apple::SingleThreadExecutor>();
}

std::unique_ptr<SubmittableExecutor> ImplementationPlatform::CreateMultiThreadExecutor(
    int max_concurrency) {
  return std::make_unique<apple::MultiThreadExecutor>(max_concurrency);
}

std::unique_ptr<ScheduledExecutor> ImplementationPlatform::CreateScheduledExecutor() {
  return std::make_unique<apple::ScheduledExecutor>();
}

// Mediums
std::unique_ptr<BluetoothAdapter> ImplementationPlatform::CreateBluetoothAdapter() {
  return std::make_unique<apple::BluetoothAdapter>();
}

std::unique_ptr<BluetoothClassicMedium> ImplementationPlatform::CreateBluetoothClassicMedium(
    api::BluetoothAdapter& adapter) {
  return nullptr;
}

std::unique_ptr<BleMedium> ImplementationPlatform::CreateBleMedium(api::BluetoothAdapter& adapter) {
  return nullptr;
}

std::unique_ptr<ble_v2::BleMedium> ImplementationPlatform::CreateBleV2Medium(
    api::BluetoothAdapter& adapter) {
  return std::make_unique<apple::BleMedium>(adapter);
}

std::unique_ptr<ServerSyncMedium> ImplementationPlatform::CreateServerSyncMedium() {
  return nullptr;
}

std::unique_ptr<WifiMedium> ImplementationPlatform::CreateWifiMedium() { return nullptr; }

std::unique_ptr<WifiLanMedium> ImplementationPlatform::CreateWifiLanMedium() {
  return std::make_unique<apple::WifiLanMedium>();
}

std::unique_ptr<WifiHotspotMedium> ImplementationPlatform::CreateWifiHotspotMedium() {
  return nullptr;
}

std::unique_ptr<WifiDirectMedium> ImplementationPlatform::CreateWifiDirectMedium() {
  return nullptr;
}

#ifndef NO_WEBRTC
std::unique_ptr<WebRtcMedium> ImplementationPlatform::CreateWebRtcMedium() { return nullptr; }
#endif

absl::StatusOr<WebResponse> ImplementationPlatform::SendRequest(const WebRequest& requestInfo) {
  NSURL* url = [NSURL URLWithString:@(requestInfo.url.c_str())];
  NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url];
  [request setHTTPMethod:@(requestInfo.method.c_str())];
  for (const auto& header : requestInfo.headers) {
    [request setValue:@(header.second.c_str()) forHTTPHeaderField:@(header.first.c_str())];
  }
  [request setHTTPBody:[@(requestInfo.body.c_str()) dataUsingEncoding:NSUTF8StringEncoding]];

  NSCondition* condition = [[NSCondition alloc] init];
  [condition lock];

  __block NSData* blockData = nil;
  __block NSHTTPURLResponse* blockResponse = nil;
  __block NSError* blockError = nil;
  NSURLSessionDataTask* task = [[NSURLSession sharedSession]
      dataTaskWithRequest:request
        completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
          [condition lock];
          blockData = data;
          if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
            blockResponse = (NSHTTPURLResponse*)response;
          }
          blockError = error;
          [condition signal];
          [condition unlock];
        }];

  [task resume];
  [condition wait];
  [condition unlock];

  if (blockResponse == nil) {
    return absl::UnknownError([[blockError localizedDescription] UTF8String]);
  }

  WebResponse webResponse;
  webResponse.status_code = blockResponse.statusCode;
  webResponse.status_text =
      [[NSHTTPURLResponse localizedStringForStatusCode:blockResponse.statusCode] UTF8String];
  NSDictionary<NSString*, NSString*>* headers = blockResponse.allHeaderFields;
  for (NSString* key in headers) {
    NSString* value = [headers objectForKey:key];
    webResponse.headers.insert({[key UTF8String], [value UTF8String]});
  }
  if (blockData != nil) {
    webResponse.body = [[[NSString alloc] initWithData:blockData
                                              encoding:NSUTF8StringEncoding] UTF8String];
  }
  return webResponse;
}

std::unique_ptr<Timer> ImplementationPlatform::CreateTimer() {
  return std::make_unique<apple::Timer>();
}

// TODO(b/261511530): Add implementation.
std::unique_ptr<nearby::api::DeviceInfo> ImplementationPlatform::CreateDeviceInfo() {
  return std::make_unique<apple::DeviceInfo>();
}

}  // namespace api
}  // namespace nearby
