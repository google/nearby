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
  GNCWiFiAwareConnectionWrapper* socket = socket_;
  if (socket == nil) {
    return ExceptionOr<ByteArray>{ByteArray()};
  }
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSData* blockData = nil;

  [socket readMaxLength:size
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
  GNCWiFiAwareConnectionWrapper* socket = socket_;
  socket_ = nil;
  [socket close];
  return {Exception::kSuccess};
}

#pragma mark - WifiAwareOutputStream

WifiAwareOutputStream::WifiAwareOutputStream(GNCWiFiAwareConnectionWrapper* socket)
    : socket_(socket) {}

Exception WifiAwareOutputStream::Write(absl::string_view data) {
  NSCAssert(![NSThread isMainThread], @"This method must not be called on the main thread");
  GNCWiFiAwareConnectionWrapper* socket = socket_;
  if (socket == nil) {
    return {Exception::kIo};
  }
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError* blockError = nil;

  NSData* nsData = [NSData dataWithBytes:data.data() length:data.size()];
  [socket write:nsData
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

Exception WifiAwareOutputStream::Close() {
  socket_ = nil;
  return {Exception::kSuccess};
}

#pragma mark - WifiAwareSocket

WifiAwareSocket::WifiAwareSocket(GNCWiFiAwareConnectionWrapper* socket)
    : socket_(socket),
      input_stream_(std::make_unique<WifiAwareInputStream>(socket)),
      output_stream_(std::make_unique<WifiAwareOutputStream>(socket)) {}

WifiAwareSocket::~WifiAwareSocket() { Close(); }

InputStream& WifiAwareSocket::GetInputStream() { return *input_stream_; }

OutputStream& WifiAwareSocket::GetOutputStream() { return *output_stream_; }

Exception WifiAwareSocket::Close() {
  if (input_stream_ != nullptr) {
    input_stream_->Close();
  }
  if (output_stream_ != nullptr) {
    output_stream_->Close();
  }
  GNCWiFiAwareConnectionWrapper* socket = socket_;
  socket_ = nil;
  [socket close];
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
  medium_ = [[GNCNWFramework alloc] initWithPeerToPeer:YES];
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

  return network_utils::StartDiscovery(medium_, service_type, std::move(network_callback));
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
    GNCAwareManager* awareManager = aware_manager_;

    // Nothing downstream of this gate needs the pairing UI to be *gone*; it only needs the pairing
    // to *exist* (browse() re-checks the paired-device store before it does anything). So release
    // on whichever arrives first. Guarded by `gateReleased` and funnelled through the main queue so
    // the two racing callbacks cannot both signal.
    __block BOOL gateReleased = NO;
    void (^releaseGate)(NSString*) = ^(NSString* source) {
      dispatch_async(dispatch_get_main_queue(), ^{
        if (gateReleased) {
          return;
        }
        gateReleased = YES;
        GNCLoggerInfo(@"[NEARBY] WifiAwareMedium Subscribing pairing gate released by: %@", source);
        dispatch_semaphore_signal(sem);
      });
    };

    NSString* peerId = [NSString stringWithUTF8String:expected_peer_id_.c_str()];
    BOOL peerIdKnown = expected_peer_id_.empty() ? NO : YES;

    dispatch_async(dispatch_get_main_queue(), ^{
      GNCWiFiAwareSwiftWrapper* awareSwiftWrapper = [[GNCWiFiAwareSwiftWrapper alloc] init];

      // Presents Apple's pairing UI and releases the gate once a pairing for `peerId` exists or the
      // sheet goes away, whichever happens first.
      void (^pairWithPeer)() = ^{
        [awareManager awaitNewPairedDeviceForPeerId:peerId
                                  completionHandler:^{
                                    releaseGate(@"new pairing recorded");
                                  }];
        if (is_showing_pairing_ui_) {
          // Android re-sends UPGRADE_PATH_AVAILABLE every few seconds, so a sheet from an earlier
          // attempt may still be up. Presenting a second one silently fails and its completion
          // never runs, which would wedge this attempt on the gate forever.
          GNCLoggerInfo(@"[NEARBY] WifiAwareMedium  Already showing subscribing pairing View, not "
                        @"stacking another");
          releaseGate(@"pairing UI already showing");
          return;
        }
        is_showing_pairing_ui_ = true;
        pairing_ui_presenter_ = awareSwiftWrapper;
        const uint64_t generation = ++pairing_ui_generation_;
        dispatch_async(dispatch_get_main_queue(), ^{
          [awareSwiftWrapper showAwareSubscribingViewAtTopWithCompletion:^{
            // If this UI was abandoned (see DismissAbandonedSubscribingPairingUi()), a newer
            // attempt may own the UI state and the pairing watch by now; leave both alone.
            if (pairing_ui_generation_ == generation) {
              // Backstop: also covers the user cancelling without ever pairing.
              is_showing_pairing_ui_ = false;
              pairing_ui_presenter_ = nil;
              // The attempt is over either way. A successful pairing has already been bound by
              // now (the store updates well before the sheet's success animation ends); an
              // abandoned one must not leave a watch behind to bind whatever gets paired next to
              // this `peerId`.
              [awareManager cancelPairingWatch];
            }
            releaseGate(@"pairing UI dismissal");
          }];
        });
      };

      if (!peerIdKnown) {
        // The peer sent no identifier, so we cannot tell which device it is. Nothing better is
        // available than the old global check: pair if we have no pairings at all, otherwise hope
        // the one we have is the right one. This is the path that fails for a second device, and
        // it only remains for peers running a build that predates the identifier.
        GNCLoggerInfo(@"[NEARBY] WifiAwareMedium Subscribing gate: peer sent no identifier, using "
                      @"paired-device count");
        [awareSwiftWrapper
            getPairedDeviceCountWithCompletionHandler:^(NSInteger pairedDeviceCount) {
              GNCLoggerInfo(@"[NEARBY] WifiAwareMedium Aware paired devices count: %@",
                            @(pairedDeviceCount));
              if (pairedDeviceCount == 0) {
                [awareManager awaitPairedDeviceWithCompletionHandler:^{
                  releaseGate(@"paired-device store");
                }];
                pairWithPeer();
              } else {
                GNCLoggerInfo(@"[NEARBY] WifiAwareMedium Skipping subscribing View, already have "
                              @"Aware paired devices %@",
                              @(pairedDeviceCount));
                releaseGate(@"cached pairing");
              }
            }];
        return;
      }

      [awareManager
          hasPairingForPeerId:peerId
            completionHandler:^(BOOL alreadyPaired) {
              GNCLoggerInfo(
                  @"[NEARBY] WifiAwareMedium Subscribing gate: peerId=%@ alreadyPaired=%@", peerId,
                  alreadyPaired ? @"YES" : @"NO");
              if (alreadyPaired) {
                // We have a pairing with *this* device. No UI needed, and browse() will
                // narrow the subscribe to it.
                releaseGate(@"known peer");
                return;
              }
              pairWithPeer();
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
  [aware_manager_ cancelPairingWatch];
  if (subscribe_pairing_semaphore_ != nil) {
    dispatch_semaphore_signal(subscribe_pairing_semaphore_);
    subscribe_pairing_semaphore_ = nil;
  }
  return true;
}

void WifiAwareMedium::SetExpectedPeerId(const std::string& peer_id) {
  GNCLoggerInfo(@"[NEARBY] WifiAwareMedium SetExpectedPeerId: %s",
                peer_id.empty() ? "(none)" : peer_id.c_str());
  expected_peer_id_ = peer_id;
}

void WifiAwareMedium::DismissAbandonedSubscribingPairingUi() {
  dispatch_async(dispatch_get_main_queue(), ^{
    if (!is_showing_pairing_ui_) {
      return;
    }
    GNCLoggerInfo(@"[NEARBY] WifiAwareMedium Dismissing abandoned subscribing pairing View");
    // Hand the UI state back right away: the dismissal callback of this UI only runs after the
    // dismissal animation and a settling delay, and the next upgrade attempt must not take the
    // "already showing" shortcut meanwhile. Bumping the generation makes that callback a no-op.
    ++pairing_ui_generation_;
    is_showing_pairing_ui_ = false;
    GNCWiFiAwareSwiftWrapper* presenter = pairing_ui_presenter_;
    pairing_ui_presenter_ = nil;
    [presenter dismissAwareSubscribingView];
  });
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
            DismissAbandonedSubscribingPairingUi();
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

  // Narrows the subscribe to the paired device for `expected_peer_id_`. An empty ID (peer sent
  // none) falls back to browsing across all paired devices.
  NSString* peerId = [NSString stringWithUTF8String:expected_peer_id_.c_str()];
  [aware_manager_
         browseWithPort:targetPort
                 peerId:peerId
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

std::unique_ptr<api::WifiAwareSocket> WifiAwareMedium::ConnectToService(
    const std::string& service_name, const ByteArray& service_info, const std::string& passphrase,
    int port, CancellationFlag* cancellation_flag) {
  NSCAssert(![NSThread isMainThread], @"This method must not be called on the main thread");
  NSLog(@"QS_AWARE ConnectToService start, serviceName: %s, serviceInfo size: %zu, "
        @"passphrase size: %zu, port: %d",
        service_name.c_str(), service_info.size(), passphrase.size(), port);

  if (cancellation_flag != nullptr && cancellation_flag->Cancelled()) {
    NSLog(@"QS_AWARE ConnectToService: already cancelled");
    return nullptr;
  }

  if (aware_manager_ == nil) {
    NSLog(@"QS_AWARE ConnectToService: aware_manager_ is nil");
    return nullptr;
  }

  if (subscribe_pairing_semaphore_ != nil) {
    NSLog(@"QS_AWARE ConnectToService: waiting for subscription pairing UI...");
    std::unique_ptr<CancellationFlagListener> pairing_cancellation_listener;
    if (cancellation_flag != nullptr) {
      pairing_cancellation_listener =
          std::make_unique<CancellationFlagListener>(cancellation_flag, [this]() {
            NSLog(@"QS_AWARE ConnectToService: cancellation requested during pairing UI");
            DismissAbandonedSubscribingPairingUi();
            [aware_manager_ cancelPairingWatch];
            if (subscribe_pairing_semaphore_ != nil) {
              dispatch_semaphore_signal(subscribe_pairing_semaphore_);
            }
          });
    }
    dispatch_semaphore_wait(subscribe_pairing_semaphore_, DISPATCH_TIME_FOREVER);
    NSLog(@"QS_AWARE ConnectToService: subscription pairing UI dismissed, proceeding.");
    subscribe_pairing_semaphore_ = nil;
  }

  if (cancellation_flag != nullptr && cancellation_flag->Cancelled()) {
    NSLog(@"QS_AWARE ConnectToService: cancelled after pairing UI");
    return nullptr;
  }

  NSLog(@"QS_AWARE ConnectToService: aware_manager_ is %@", aware_manager_);
  dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  __block NSError* browseError = nil;

  std::unique_ptr<CancellationFlagListener> cancellation_flag_listener;
  if (cancellation_flag != nullptr) {
    cancellation_flag_listener =
        std::make_unique<CancellationFlagListener>(cancellation_flag, [this, semaphore]() {
          NSLog(@"QS_AWARE ConnectToService: cancellation requested, cancelling browse task");
          [aware_manager_ cancelBrowseTask];
          dispatch_semaphore_signal(semaphore);
        });
  }

  NSString* peerId = [NSString stringWithUTF8String:expected_peer_id_.c_str()];
  NSLog(@"QS_AWARE ConnectToService: calling browseWithPort:%d peerId:%@ completionHandler", port,
        peerId);
  [aware_manager_
         browseWithPort:port
                 peerId:peerId
      completionHandler:^(NSError* _Nullable error) {
        NSLog(@"QS_AWARE ConnectToService: browseWithPort completed with error: %@", error);
        browseError = error;
        dispatch_semaphore_signal(semaphore);
      }];

  dispatch_time_t timeout = dispatch_time(DISPATCH_TIME_NOW, 40 * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(semaphore, timeout) != 0) {
    NSLog(@"QS_AWARE ConnectToService: browseWithPort timed out (40s)");
    [aware_manager_ cancelBrowseTask];
    return nullptr;
  }

  if (cancellation_flag != nullptr && cancellation_flag->Cancelled()) {
    NSLog(@"QS_AWARE ConnectToService: cancelled during browse");
    [aware_manager_ cancelBrowseTask];
    return nullptr;
  }

  if (browseError != nil) {
    NSLog(@"QS_AWARE Error during GNCAwareManager browse: %@", browseError);
    return nullptr;
  }

  GNCWiFiAwareConnectionWrapper* connectionWrapper = [aware_manager_ getLatestConnectionWrapper];
  NSLog(@"QS_AWARE ConnectToService: connectionWrapper is %@", connectionWrapper);
  if (connectionWrapper == nil) {
    NSLog(@"QS_AWARE Error: GNCAwareManager browse succeeded but latestConnectionWrapper is nil");
    return nullptr;
  }

  NSLog(@"QS_AWARE ConnectToService completed successfully.");
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

WifiAwareSocket::~WifiAwareSocket() = default;

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

void WifiAwareMedium::SetExpectedPeerId(const std::string& peer_id) {}

std::unique_ptr<api::WifiAwareSocket> WifiAwareMedium::ConnectToService(
    const WifiAwareServiceInfo& remote_service_info, CancellationFlag* cancellation_flag) {
  return nullptr;
}

std::unique_ptr<api::WifiAwareSocket> WifiAwareMedium::ConnectToService(
    const std::string& service_name, const ByteArray& service_info, const std::string& passphrase,
    int port, CancellationFlag* cancellation_flag) {
  return nullptr;
}

std::unique_ptr<api::WifiAwareServerSocket> WifiAwareMedium::ListenForService(int port) {
  return nullptr;
}

#endif  // TARGET_OS_IOS && !defined(GITHUB_BUILD)

}  // namespace apple
}  // namespace nearby

#pragma clang diagnostic pop
