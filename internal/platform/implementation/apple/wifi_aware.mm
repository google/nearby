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

#import "internal/platform/implementation/apple/wifi_aware.h"

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>
#import <dispatch/dispatch.h>

#include <memory>
#include <string>
#include <utility>

#include "internal/platform/cancellation_flag_listener.h"

#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/GNCNWFramework.h"

#import "internal/platform/implementation/apple/network_utils.h"

#if TARGET_OS_IOS && !defined(GITHUB_BUILD)
#import "internal/platform/implementation/apple/Mediums/Aware/WiFiAwareMedium-Swift.h"
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"

namespace nearby {
namespace apple {

#if TARGET_OS_IOS && !defined(GITHUB_BUILD)

#pragma mark - WifiAwareInputStream

WifiAwareInputStream::WifiAwareInputStream(GNCWiFiAwareConnectionWrapper* socket)
    : socket_(socket) {}

ExceptionOr<ByteArray> WifiAwareInputStream::Read(std::int64_t size) {
  NSCAssert(![NSThread isMainThread], @"This method must not be called on the main thread");
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSData* blockData = nil;

  [socket_ readMaxLength:size
       completionHandler:^(NSData* _Nullable data) {
         blockData = data;
         dispatch_semaphore_signal(semaphore);
       }];

  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

  if (blockData == nil) {
    return ExceptionOr<ByteArray>{ByteArray()};
  }

  return ExceptionOr<ByteArray>{ByteArray((const char*)blockData.bytes, blockData.length)};
}

Exception WifiAwareInputStream::Close() {
  [socket_ close];
  return {Exception::kSuccess};
}

#pragma mark - WifiAwareOutputStream

WifiAwareOutputStream::WifiAwareOutputStream(GNCWiFiAwareConnectionWrapper* socket)
    : socket_(socket) {}

Exception WifiAwareOutputStream::Write(absl::string_view data) {
  NSCAssert(![NSThread isMainThread], @"This method must not be called on the main thread");
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError* blockError = nil;

  NSData* nsData = [NSData dataWithBytes:data.data() length:data.size()];
  [socket_ write:nsData
      completionHandler:^(NSError* _Nullable error) {
        blockError = error;
        dispatch_semaphore_signal(semaphore);
      }];

  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

  if (blockError != nil) {
    GNCLoggerError(@"[NEARBY] WifiAwareOutputStream Write error: %@", blockError);
    return {Exception::kIo};
  }
  return {Exception::kSuccess};
}

Exception WifiAwareOutputStream::Flush() { return {Exception::kSuccess}; }

Exception WifiAwareOutputStream::Close() { return {Exception::kSuccess}; }

#pragma mark - WifiAwareSocket

WifiAwareSocket::WifiAwareSocket(GNCWiFiAwareConnectionWrapper* socket)
    : socket_(socket),
      input_stream_(std::make_unique<WifiAwareInputStream>(socket)),
      output_stream_(std::make_unique<WifiAwareOutputStream>(socket)) {}

InputStream& WifiAwareSocket::GetInputStream() { return *input_stream_; }

OutputStream& WifiAwareSocket::GetOutputStream() { return *output_stream_; }

Exception WifiAwareSocket::Close() {
  [socket_ close];
  return {Exception::kSuccess};
}

#pragma mark - WifiAwareServerSocket

class WifiAwareServerSocket : public api::WifiAwareServerSocket {
 public:
  WifiAwareServerSocket(GNCWiFiAwareConnectionWrapper* listener_wrapper,
                        GNCAwareManager* aware_manager, dispatch_semaphore_t pairing_semaphore)
      : listener_wrapper_(listener_wrapper),
        aware_manager_(aware_manager),
        pairing_semaphore_(pairing_semaphore) {}
  ~WifiAwareServerSocket() override = default;

  std::unique_ptr<api::WifiAwareSocket> Accept() override {
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

  Exception Close() override {
    GNCLoggerInfo(@"[NEARBY] WifiAwareServerSocket Close");
    if (pairing_semaphore_ != nil) {
      dispatch_semaphore_signal(pairing_semaphore_);
      pairing_semaphore_ = nil;
    }
    [aware_manager_ cancelListenTask];
    [listener_wrapper_ close];
    return {Exception::kSuccess};
  }

 private:
  GNCWiFiAwareConnectionWrapper* listener_wrapper_;
  GNCAwareManager* aware_manager_;
  dispatch_semaphore_t pairing_semaphore_;
  bool listen_started_ = false;
};

#pragma mark - WifiAwareMedium

WifiAwareMedium::WifiAwareMedium() {
  medium_ = [[GNCNWFramework alloc] init];
  if (@available(iOS 26.0, *)) {
    aware_manager_ = [[GNCAwareManager alloc] init];
  } else {
    aware_manager_ = nil;
  }
}

WifiAwareMedium::WifiAwareMedium(GNCNWFramework* medium) : medium_(medium) {}

bool WifiAwareMedium::StartAdvertising(const WifiAwareServiceInfo& wifi_aware_service_info) {
  return network_utils::StartAdvertising(medium_,
                                         static_cast<NsdServiceInfo>(wifi_aware_service_info));
}

bool WifiAwareMedium::StopAdvertising(const WifiAwareServiceInfo& wifi_aware_service_info) {
  return network_utils::StopAdvertising(medium_,
                                        static_cast<NsdServiceInfo>(wifi_aware_service_info));
}

bool WifiAwareMedium::StartDiscovery(const std::string& service_type,
                                     DiscoveredServiceCallback callback) {
  service_callback_ = std::move(callback);
  network_utils::NetworkDiscoveredServiceCallback network_callback = {
      .network_service_discovered_cb =
          [this](const NsdServiceInfo& nsd_info) {
            service_callback_.service_discovered_cb(WifiAwareServiceInfo(nsd_info));
          },
      .network_service_lost_cb =
          [this](const NsdServiceInfo& nsd_info) {
            service_callback_.service_lost_cb(WifiAwareServiceInfo(nsd_info));
          }};

  return network_utils::StartDiscovery(medium_, service_type, std::move(network_callback),
                                       /*include_peer_to_peer=*/true);
}

bool WifiAwareMedium::StopDiscovery(const std::string& service_type) {
  return network_utils::StopDiscovery(medium_, service_type);
}

bool WifiAwareMedium::IsPublishing() { return is_publishing_; }

bool WifiAwareMedium::StartPublishing() {
  GNCLoggerInfo(@"[NEARBY] WifiAwareMedium StartPublishing");
  if (is_publishing_) {
    return true;
  }

  if (@available(iOS 26.0, *)) {
    NSCAssert(![NSThread isMainThread], @"This method must not be called on the main thread");
    publish_pairing_semaphore_ = dispatch_semaphore_create(0);
    dispatch_semaphore_t sem = publish_pairing_semaphore_;

    dispatch_async(dispatch_get_main_queue(), ^{
      GNCWiFiAwareMedium* awareMedium = [[GNCWiFiAwareMedium alloc] init];
      [awareMedium getPairedDeviceCountWithCompletionHandler:^(NSInteger pairedDeviceCount) {
        GNCLoggerInfo(@"[NEARBY] Aware paired devices count: %@", @(pairedDeviceCount));
        if (pairedDeviceCount == 0) {
          dispatch_async(dispatch_get_main_queue(), ^{
            [awareMedium showAwarePublishingViewAtTopWithCompletion:^{
              GNCLoggerInfo(
                  @"[NEARBY] showAwarePublishingViewAtTop dismissed, signaling semaphore");
              dispatch_semaphore_signal(sem);
            }];
          });
        } else {
          GNCLoggerInfo(@"[NEARBY] Skipping publishing View, already have Aware paired devices %@",
                        @(pairedDeviceCount));
          dispatch_semaphore_signal(sem);
        }
      }];
    });

  } else {
    GNCLoggerError(@"[NEARBY] Wi-Fi Aware publishing not supported on this iOS version");
    return false;
  }

  is_publishing_ = true;
  return true;
}
bool WifiAwareMedium::StopPublishing() {
  GNCLoggerInfo(@"[NEARBY] WifiAwareMedium StopPublishing");
  is_publishing_ = false;
  [aware_manager_ cancelListenTask];
  if (publish_pairing_semaphore_ != nil) {
    dispatch_semaphore_signal(publish_pairing_semaphore_);
    publish_pairing_semaphore_ = nil;
  }
  return true;
}

bool WifiAwareMedium::IsSubscribing() { return is_subscribing_; }
bool WifiAwareMedium::StartSubscribing() {
  GNCLoggerInfo(@"[NEARBY] WifiAwareMedium StartSubscribing");
  if (is_subscribing_) {
    return true;
  }

  if (@available(iOS 26.0, *)) {
    NSCAssert(![NSThread isMainThread], @"This method must not be called on the main thread");
    subscribe_pairing_semaphore_ = dispatch_semaphore_create(0);
    dispatch_semaphore_t sem = subscribe_pairing_semaphore_;

    dispatch_async(dispatch_get_main_queue(), ^{
      GNCWiFiAwareMedium* awareMedium = [[GNCWiFiAwareMedium alloc] init];
      [awareMedium getPairedDeviceCountWithCompletionHandler:^(NSInteger pairedDeviceCount) {
        GNCLoggerInfo(@"[NEARBY] Aware paired devices count: %@", @(pairedDeviceCount));
        if (pairedDeviceCount == 0) {
          dispatch_async(dispatch_get_main_queue(), ^{
            [awareMedium showAwareSubscribingViewAtTopWithCompletion:^{
              GNCLoggerInfo(
                  @"[NEARBY] showAwareSubscribingViewAtTop dismissed, signaling semaphore");
              dispatch_semaphore_signal(sem);
            }];
          });
        } else {
          GNCLoggerInfo(@"[NEARBY] Skipping subscribing View, already have Aware paired devices %@",
                        @(pairedDeviceCount));
          dispatch_semaphore_signal(sem);
        }
      }];
    });

  } else {
    GNCLoggerError(@"[NEARBY] Wi-Fi Aware subscribing not supported on this iOS version");
    return false;
  }

  is_subscribing_ = true;
  return true;
}
bool WifiAwareMedium::StopSubscribing() {
  GNCLoggerInfo(@"[NEARBY] WifiAwareMedium StopSubscribing");
  is_subscribing_ = false;
  [aware_manager_ cancelBrowseTask];
  if (subscribe_pairing_semaphore_ != nil) {
    dispatch_semaphore_signal(subscribe_pairing_semaphore_);
    subscribe_pairing_semaphore_ = nil;
  }
  return true;
}

std::unique_ptr<api::WifiAwareSocket> WifiAwareMedium::ConnectToService(
    const WifiAwareServiceInfo& remote_service_info, CancellationFlag* cancellation_flag) {
  NSCAssert(![NSThread isMainThread], @"This method must not be called on the main thread");
  int targetPort = remote_service_info.GetPort();
  GNCLoggerInfo(
      @"[NEARBY] ConnectToService start, serviceName: %s, serviceType: %s, targetPort: %d",
      remote_service_info.GetServiceName().c_str(), remote_service_info.GetServiceType().c_str(),
      targetPort);

  if (cancellation_flag != nullptr && cancellation_flag->Cancelled()) {
    GNCLoggerInfo(@"[NEARBY] ConnectToService: already cancelled");
    return nullptr;
  }

  if (aware_manager_ == nil) {
    GNCLoggerError(@"[NEARBY] ConnectToService: aware_manager_ is nil");
    return nullptr;
  }

  if (subscribe_pairing_semaphore_ != nil) {
    GNCLoggerInfo(@"[NEARBY] ConnectToService: waiting for subscription pairing UI...");
    std::unique_ptr<CancellationFlagListener> pairing_cancellation_listener;
    if (cancellation_flag != nullptr) {
      pairing_cancellation_listener =
          std::make_unique<CancellationFlagListener>(cancellation_flag, [this]() {
            GNCLoggerInfo(@"[NEARBY] ConnectToService: cancellation requested during pairing UI");
            if (subscribe_pairing_semaphore_ != nil) {
              dispatch_semaphore_signal(subscribe_pairing_semaphore_);
            }
          });
    }
    dispatch_semaphore_wait(subscribe_pairing_semaphore_, DISPATCH_TIME_FOREVER);
    GNCLoggerInfo(@"[NEARBY] ConnectToService: subscription pairing UI dismissed, proceeding.");
    subscribe_pairing_semaphore_ = nil;
  }

  if (cancellation_flag != nullptr && cancellation_flag->Cancelled()) {
    GNCLoggerInfo(@"[NEARBY] ConnectToService: cancelled after pairing UI");
    return nullptr;
  }

  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError* browseError = nil;

  std::unique_ptr<CancellationFlagListener> cancellation_flag_listener;
  if (cancellation_flag != nullptr) {
    cancellation_flag_listener =
        std::make_unique<CancellationFlagListener>(cancellation_flag, [this, semaphore]() {
          GNCLoggerInfo(
              @"[NEARBY] ConnectToService: cancellation requested, cancelling browse task");
          [aware_manager_ cancelBrowseTask];
          dispatch_semaphore_signal(semaphore);
        });
  }

  [aware_manager_
         browseWithPort:targetPort
      completionHandler:^(NSError* _Nullable error) {
        if (error != nil) {
          GNCLoggerError(@"[NEARBY] ConnectToService: browseWithPort completed with error: %@",
                         error);
        }
        browseError = error;
        dispatch_semaphore_signal(semaphore);
      }];

  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 40 * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    GNCLoggerError(@"[NEARBY] ConnectToService: browseWithPort timed out (40s)");
    [aware_manager_ cancelBrowseTask];
    return nullptr;
  }

  if (cancellation_flag != nullptr && cancellation_flag->Cancelled()) {
    GNCLoggerInfo(@"[NEARBY] ConnectToService: cancelled during browse");
    [aware_manager_ cancelBrowseTask];
    return nullptr;
  }

  if (browseError != nil) {
    GNCLoggerError(@"[NEARBY] Error during GNCAwareManager browse: %@", browseError);
    return nullptr;
  }

  GNCWiFiAwareConnectionWrapper* connectionWrapper = [aware_manager_ getLatestConnectionWrapper];
  if (connectionWrapper == nil) {
    GNCLoggerError(
        @"[NEARBY] Error: GNCAwareManager browse succeeded but latestConnectionWrapper is nil");
    return nullptr;
  }

  GNCLoggerInfo(@"[NEARBY] ConnectToService completed successfully.");
  return std::make_unique<WifiAwareSocket>(connectionWrapper);
}

std::unique_ptr<api::WifiAwareServerSocket> WifiAwareMedium::ListenForService(int port) {
  GNCLoggerInfo(@"[NEARBY] WifiAwareMedium ListenForService start on port %d", port);
  if (aware_manager_ == nil) {
    GNCLoggerError(@"[NEARBY] WifiAwareMedium ListenForService: aware_manager_ is nil");
    return nullptr;
  }
  GNCWiFiAwareConnectionWrapper* listener_wrapper = aware_manager_.latestListenerWrapper;
  auto server_socket_ptr = std::make_unique<WifiAwareServerSocket>(listener_wrapper, aware_manager_,
                                                                   publish_pairing_semaphore_);

  return server_socket_ptr;
}

#else  // !(TARGET_OS_IOS && !defined(GITHUB_BUILD))

WifiAwareInputStream::WifiAwareInputStream(GNCWiFiAwareConnectionWrapper* socket)
    : socket_(socket) {}

ExceptionOr<ByteArray> WifiAwareInputStream::Read(std::int64_t size) {
  return ExceptionOr<ByteArray>{ByteArray()};
}

Exception WifiAwareInputStream::Close() { return {Exception::kSuccess}; }

WifiAwareOutputStream::WifiAwareOutputStream(GNCWiFiAwareConnectionWrapper* socket)
    : socket_(socket) {}

Exception WifiAwareOutputStream::Write(absl::string_view data) { return {Exception::kIo}; }

Exception WifiAwareOutputStream::Flush() { return {Exception::kSuccess}; }

Exception WifiAwareOutputStream::Close() { return {Exception::kSuccess}; }

WifiAwareSocket::WifiAwareSocket(GNCWiFiAwareConnectionWrapper* socket)
    : socket_(socket),
      input_stream_(std::make_unique<WifiAwareInputStream>(socket)),
      output_stream_(std::make_unique<WifiAwareOutputStream>(socket)) {}

InputStream& WifiAwareSocket::GetInputStream() { return *input_stream_; }

OutputStream& WifiAwareSocket::GetOutputStream() { return *output_stream_; }

Exception WifiAwareSocket::Close() { return {Exception::kSuccess}; }

WifiAwareMedium::WifiAwareMedium() : medium_(nil), aware_manager_(nil) {}

WifiAwareMedium::WifiAwareMedium(GNCNWFramework* medium) : medium_(medium), aware_manager_(nil) {}

bool WifiAwareMedium::StartAdvertising(const WifiAwareServiceInfo& wifi_aware_service_info) {
  return false;
}

bool WifiAwareMedium::StopAdvertising(const WifiAwareServiceInfo& wifi_aware_service_info) {
  return false;
}

bool WifiAwareMedium::StartDiscovery(const std::string& service_type,
                                     DiscoveredServiceCallback callback) {
  return false;
}

bool WifiAwareMedium::StopDiscovery(const std::string& service_type) { return false; }

bool WifiAwareMedium::IsPublishing() { return false; }

bool WifiAwareMedium::StartPublishing() { return false; }

bool WifiAwareMedium::StopPublishing() { return false; }

bool WifiAwareMedium::IsSubscribing() { return false; }

bool WifiAwareMedium::StartSubscribing() { return false; }

bool WifiAwareMedium::StopSubscribing() { return false; }

std::unique_ptr<api::WifiAwareSocket> WifiAwareMedium::ConnectToService(
    const WifiAwareServiceInfo& remote_service_info, CancellationFlag* cancellation_flag) {
  return nullptr;
}

std::unique_ptr<api::WifiAwareServerSocket> WifiAwareMedium::ListenForService(int port) {
  return nullptr;
}

#endif  // TARGET_OS_IOS && !defined(GITHUB_BUILD)

}  // namespace apple
}  // namespace nearby

#pragma clang diagnostic pop
