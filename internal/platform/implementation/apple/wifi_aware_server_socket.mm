// Copyright 2026 Google LLC
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

#import "internal/platform/implementation/apple/wifi_aware_server_socket.h"

#import <TargetConditionals.h>
#import <dispatch/dispatch.h>

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#if TARGET_OS_IOS
#import "internal/platform/implementation/apple/WiFiAwareSwiftWrapper-Swift.h"
#endif
#import "internal/platform/implementation/apple/wifi_aware.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"

namespace nearby {
namespace apple {
#pragma mark - WifiAwareServerSocket

#if TARGET_OS_IOS
WifiAwareServerSocket::WifiAwareServerSocket(GNCWiFiAwareConnectionWrapper* listener_wrapper,
                                             GNCAwareManager* aware_manager,
                                             dispatch_semaphore_t pairing_semaphore)
    : listener_wrapper_(listener_wrapper),
      aware_manager_(aware_manager),
      pairing_semaphore_(pairing_semaphore) {}

std::unique_ptr<api::WifiAwareSocket> WifiAwareServerSocket::Accept() {
  if (listener_wrapper_ == nil) {
    return nullptr;
  }
  NSCAssert(![NSThread isMainThread], @"This method must not be called on the main thread");

  if (pairing_semaphore_ != nil) {
    GNCLoggerInfo(@"[NEARBY] WifiAwareServerSocket Accept: waiting for pairing UI...");
    dispatch_semaphore_wait(pairing_semaphore_, DISPATCH_TIME_FOREVER);
    GNCLoggerInfo(@"[NEARBY] WifiAwareServerSocket Accept: pairing UI dismissed, proceeding.");
    pairing_semaphore_ = nil;
  }

  if (!listen_started_) {
    dispatch_semaphore_t listen_semaphore = dispatch_semaphore_create(0);
    __block NSError* listen_error = nil;
    [aware_manager_ listenWithCompletionHandler:^(NSError* _Nullable error) {
      listen_error = error;
      dispatch_semaphore_signal(listen_semaphore);
    }];
    dispatch_semaphore_wait(listen_semaphore, DISPATCH_TIME_FOREVER);
    if (listen_error != nil) {
      GNCLoggerError(@"[NEARBY] WifiAwareServerSocket Accept: listen failed: %@", listen_error);
      return nullptr;
    }
    listen_started_ = true;
  }

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block GNCWiFiAwareConnectionWrapper* accepted_wrapper = nil;

  [listener_wrapper_
      acceptConnectionWithCompletionHandler:^(GNCWiFiAwareConnectionWrapper* _Nullable wrapper) {
        accepted_wrapper = wrapper;
        dispatch_semaphore_signal(semaphore);
      }];

  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

  if (accepted_wrapper == nil) {
    GNCLoggerError(@"[NEARBY] WifiAwareServerSocket Accept failed or listener closed");
    return nullptr;
  }

  return std::make_unique<WifiAwareSocket>(accepted_wrapper);
}

Exception WifiAwareServerSocket::Close() {
  GNCLoggerInfo(@"[NEARBY] WifiAwareServerSocket Close");
  if (pairing_semaphore_ != nil) {
    dispatch_semaphore_signal(pairing_semaphore_);
    pairing_semaphore_ = nil;
  }
  [aware_manager_ cancelListenTask];
  [listener_wrapper_ close];
  return {Exception::kSuccess};
}
#else
WifiAwareServerSocket::WifiAwareServerSocket(GNCWiFiAwareConnectionWrapper* listener_wrapper,
                                             GNCAwareManager* aware_manager,
                                             dispatch_semaphore_t pairing_semaphore)
    : listener_wrapper_(listener_wrapper),
      aware_manager_(aware_manager),
      pairing_semaphore_(pairing_semaphore) {}

std::unique_ptr<api::WifiAwareSocket> WifiAwareServerSocket::Accept() { return nullptr; }

Exception WifiAwareServerSocket::Close() { return {Exception::kSuccess}; }
#endif

}  // namespace apple
}  // namespace nearby

#pragma clang diagnostic pop
