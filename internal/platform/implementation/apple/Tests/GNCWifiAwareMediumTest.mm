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

#include "internal/platform/implementation/apple/wifi_aware.h"

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>
#import <XCTest/XCTest.h>

#include <memory>
#include <string>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#import "internal/platform/implementation/apple/Mediums/WiFiCommon/Tests/GNCFakeNWFramework.h"
#include "internal/platform/nsd_service_info.h"

#if TARGET_OS_IOS && !defined(GITHUB_BUILD)
#import "internal/platform/implementation/apple/Mediums/Aware/WiFiAwareMedium-Swift.h"
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"

static NSString* const kTestServiceType = @"_qs-aware._tcp";
static NSString* const kTestServiceName = @"TestAwareService";
static const int kTestPort = 5678;

#if TARGET_OS_IOS && !defined(GITHUB_BUILD)
/**
 * Lightweight fake stand-in for `GNCWiFiAwareConnectionWrapper` used to verify Objective-C++
 * `WifiAwareSocket`, `WifiAwareInputStream`, and `WifiAwareOutputStream` lifecycle and I/O
 * independent of the iOS runtime version.
 */
@interface GNCFakeWiFiAwareConnectionWrapper : NSObject
@property(nonatomic, assign) NSInteger closeCallCount;
@property(nonatomic, copy, nullable) NSData* dataToReturnOnRead;
@property(nonatomic, strong, readonly) NSMutableData* writtenData;
@property(nonatomic, strong, nullable) NSError* errorToReturnOnWrite;

- (void)readMaxLength:(NSInteger)length
    completionHandler:(void (^)(NSData* _Nullable data))completionHandler;
- (void)write:(NSData*)data completionHandler:(void (^)(NSError* _Nullable error))completionHandler;
- (void)close;
@end

@implementation GNCFakeWiFiAwareConnectionWrapper

- (instancetype)init {
  self = [super init];
  if (self) {
    _writtenData = [[NSMutableData alloc] init];
  }
  return self;
}

- (void)readMaxLength:(NSInteger)length
    completionHandler:(void (^)(NSData* _Nullable data))completionHandler {
  NSData* data = self.dataToReturnOnRead;
  self.dataToReturnOnRead = nil;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    completionHandler(data);
  });
}

- (void)write:(NSData*)data
    completionHandler:(void (^)(NSError* _Nullable error))completionHandler {
  if (self.errorToReturnOnWrite == nil && data != nil) {
    @synchronized(self.writtenData) {
      [self.writtenData appendData:data];
    }
  }
  NSError* error = self.errorToReturnOnWrite;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    completionHandler(error);
  });
}

- (void)close {
  @synchronized(self) {
    _closeCallCount++;
  }
}

@end
#endif  // TARGET_OS_IOS && !defined(GITHUB_BUILD)

@interface GNCWifiAwareMediumTest : XCTestCase
@end

@implementation GNCWifiAwareMediumTest {
  GNCFakeNWFramework* _fakeNWFramework;
  std::unique_ptr<nearby::apple::WifiAwareMedium> _wifiAwareMedium;
}

- (void)setUp {
  [super setUp];
  _fakeNWFramework = [[GNCFakeNWFramework alloc] init];
  _wifiAwareMedium = std::make_unique<nearby::apple::WifiAwareMedium>(_fakeNWFramework);
}

- (void)tearDown {
  _wifiAwareMedium.reset();
  [super tearDown];
}

- (void)testStartAndStopAdvertising {
  nearby::NsdServiceInfo nsdServiceInfo;
  nsdServiceInfo.SetServiceType([kTestServiceType UTF8String]);
  nsdServiceInfo.SetServiceName([kTestServiceName UTF8String]);
  nsdServiceInfo.SetPort(kTestPort);
  nearby::WifiAwareServiceInfo wifiAwareServiceInfo(nsdServiceInfo);

  XCTAssertTrue(_wifiAwareMedium->StartAdvertising(wifiAwareServiceInfo));
  XCTAssertEqualObjects(_fakeNWFramework.startedAdvertisingPort, @(kTestPort));
  XCTAssertEqualObjects(_fakeNWFramework.startedAdvertisingServiceName, kTestServiceName);
  XCTAssertEqualObjects(_fakeNWFramework.startedAdvertisingServiceType, kTestServiceType);

  XCTAssertTrue(_wifiAwareMedium->StopAdvertising(wifiAwareServiceInfo));
  XCTAssertEqualObjects(_fakeNWFramework.stoppedAdvertisingPort, @(kTestPort));
}

- (void)testStartAndStopDiscovery {
  std::string serviceType = [kTestServiceType UTF8String];

  XCTAssertTrue(_wifiAwareMedium->StartDiscovery(
      serviceType, nearby::api::WifiAwareMedium::DiscoveredServiceCallback{}));
  XCTAssertEqualObjects(_fakeNWFramework.startedDiscoveryServiceType, kTestServiceType);

  XCTAssertTrue(_wifiAwareMedium->StopDiscovery(serviceType));
  XCTAssertEqualObjects(_fakeNWFramework.stoppedDiscoveryServiceType, kTestServiceType);
}

#if TARGET_OS_IOS && !defined(GITHUB_BUILD)

- (void)testSocketDestructorClosesAndReleasesUnderlyingWrapper {
  __weak GNCFakeWiFiAwareConnectionWrapper* weakFakeWrapper = nil;
  @autoreleasepool {
    GNCFakeWiFiAwareConnectionWrapper* fakeWrapper =
        [[GNCFakeWiFiAwareConnectionWrapper alloc] init];
    weakFakeWrapper = fakeWrapper;

    auto socket = std::make_unique<nearby::apple::WifiAwareSocket>(
        (GNCWiFiAwareConnectionWrapper*)fakeWrapper);

    // Destroying WifiAwareSocket without an explicit Close() must still call -close on the
    // underlying wrapper and release all strong references held by WifiAwareSocket,
    // WifiAwareInputStream, and WifiAwareOutputStream.
    socket.reset();
    XCTAssertEqual(fakeWrapper.closeCallCount, 2);
    fakeWrapper = nil;
  }
  XCTAssertNil(weakFakeWrapper,
               @"Underlying wrapper must be deallocated when WifiAwareSocket is destroyed");
}

- (void)testSocketExplicitCloseReleasesUnderlyingWrapperAndPreventsFurtherIO {
  __weak GNCFakeWiFiAwareConnectionWrapper* weakFakeWrapper = nil;
  std::unique_ptr<nearby::apple::WifiAwareSocket> socket;
  @autoreleasepool {
    GNCFakeWiFiAwareConnectionWrapper* fakeWrapper =
        [[GNCFakeWiFiAwareConnectionWrapper alloc] init];
    weakFakeWrapper = fakeWrapper;
    socket = std::make_unique<nearby::apple::WifiAwareSocket>(
        (GNCWiFiAwareConnectionWrapper*)fakeWrapper);
    fakeWrapper = nil;

    XCTAssertEqual(socket->Close().value, nearby::Exception::kSuccess);
  }
  XCTAssertNil(
      weakFakeWrapper,
      @"Underlying wrapper must be deallocated immediately after WifiAwareSocket::Close()");

  nearby::apple::WifiAwareSocket* rawSocket = socket.get();
  XCTestExpectation* ioAfterCloseExpectation =
      [self expectationWithDescription:@"I/O after Close completes immediately"];
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    nearby::ExceptionOr<nearby::ByteArray> readResult = rawSocket->GetInputStream().Read(64);
    XCTAssertTrue(readResult.ok());
    XCTAssertTrue(readResult.result().Empty());

    nearby::Exception writeResult = rawSocket->GetOutputStream().Write("test");
    XCTAssertEqual(writeResult.value, nearby::Exception::kIo);
    [ioAfterCloseExpectation fulfill];
  });
  [self waitForExpectations:@[ ioAfterCloseExpectation ] timeout:2.0];
}

- (void)testSocketReadAndWriteStreams {
  GNCFakeWiFiAwareConnectionWrapper* fakeWrapper = [[GNCFakeWiFiAwareConnectionWrapper alloc] init];
  NSData* expectedReadData = [@"hello-aware" dataUsingEncoding:NSUTF8StringEncoding];
  fakeWrapper.dataToReturnOnRead = expectedReadData;

  auto socket =
      std::make_unique<nearby::apple::WifiAwareSocket>((GNCWiFiAwareConnectionWrapper*)fakeWrapper);
  nearby::apple::WifiAwareSocket* rawSocket = socket.get();

  XCTestExpectation* ioExpectation = [self expectationWithDescription:@"Read and Write succeed"];
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
    nearby::ExceptionOr<nearby::ByteArray> readResult = rawSocket->GetInputStream().Read(64);
    XCTAssertTrue(readResult.ok());
    XCTAssertEqual(std::string(readResult.result()), "hello-aware");

    nearby::Exception writeResult = rawSocket->GetOutputStream().Write("outbound-payload");
    XCTAssertEqual(writeResult.value, nearby::Exception::kSuccess);
    [ioExpectation fulfill];
  });
  [self waitForExpectations:@[ ioExpectation ] timeout:2.0];

  NSString* writtenString = [[NSString alloc] initWithData:fakeWrapper.writtenData
                                                  encoding:NSUTF8StringEncoding];
  XCTAssertEqualObjects(writtenString, @"outbound-payload");
}

- (void)testConnectionWrapperAwaitReadyTimeoutDoesNotHang {
  XCTestExpectation* timeoutExpectation =
      [self expectationWithDescription:@"awaitReady times out promptly via task cancellation"];
  GNCWiFiAwareConnectionWrapper* wrapper =
      [[GNCWiFiAwareConnectionWrapper alloc] initForTestingWithOnClose:^{
      }];

  CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();
  [wrapper awaitReadyWithTimeoutSeconds:0.1
                      completionHandler:^(NSError* _Nullable error) {
                        CFAbsoluteTime elapsed = CFAbsoluteTimeGetCurrent() - startTime;
                        XCTAssertNotNil(error);
                        XCTAssertEqual(error.code, -2);
                        XCTAssertLessThan(elapsed, 2.0,
                                          @"awaitReady must not hang when timeout expires");
                        [timeoutExpectation fulfill];
                      }];

  [self waitForExpectations:@[ timeoutExpectation ] timeout:2.5];
}

- (void)testConnectionWrapperAwaitReadyFailsWithSignalClosedReason {
  XCTestExpectation* closedExpectation =
      [self expectationWithDescription:@"awaitReady fails when signalClosed is called"];
  XCTestExpectation* onCloseExpectation =
      [self expectationWithDescription:@"onClose callback invoked"];

  GNCWiFiAwareConnectionWrapper* wrapper =
      [[GNCWiFiAwareConnectionWrapper alloc] initForTestingWithOnClose:^{
        [onCloseExpectation fulfill];
      }];

  [wrapper awaitReadyWithTimeoutSeconds:5.0
                      completionHandler:^(NSError* _Nullable error) {
                        XCTAssertNotNil(error);
                        XCTAssertTrue([error.localizedDescription containsString:@"-11987"]);
                        [closedExpectation fulfill];
                      }];

  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(50 * NSEC_PER_MSEC)),
                 dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
                   [wrapper signalClosedWithReason:@"Wi-Fi Aware data path refused (-11987)"];
                 });

  [self waitForExpectations:@[ closedExpectation, onCloseExpectation ] timeout:2.0];
}

- (void)testConnectionWrapperSignalReadyAndRead {
  XCTestExpectation* readyExpectation = [self expectationWithDescription:@"awaitReady succeeds"];
  XCTestExpectation* readExpectation = [self expectationWithDescription:@"readMaxLength succeeds"];

  GNCWiFiAwareConnectionWrapper* wrapper =
      [[GNCWiFiAwareConnectionWrapper alloc] initForTestingWithOnClose:^{
      }];

  [wrapper awaitReadyWithTimeoutSeconds:5.0
                      completionHandler:^(NSError* _Nullable error) {
                        XCTAssertNil(error);
                        [readyExpectation fulfill];
                      }];
  [wrapper signalReady];
  [self waitForExpectations:@[ readyExpectation ] timeout:2.0];

  NSData* testPayload = [@"aware-data-frame" dataUsingEncoding:NSUTF8StringEncoding];
  [wrapper appendIncomingData:testPayload
            completionHandler:^{
              [wrapper readMaxLength:64
                   completionHandler:^(NSData* _Nullable data) {
                     XCTAssertEqualObjects(data, testPayload);
                     [readExpectation fulfill];
                   }];
            }];
  [self waitForExpectations:@[ readExpectation ] timeout:2.0];
}

- (void)testAwareManagerGetLatestConnectionWrapperConsumesReference {
  GNCAwareManager* manager = [[GNCAwareManager alloc] init];
  __weak GNCWiFiAwareConnectionWrapper* weakWrapper = nil;

  @autoreleasepool {
    GNCWiFiAwareConnectionWrapper* wrapper =
        [[GNCWiFiAwareConnectionWrapper alloc] initForTestingWithOnClose:^{
        }];
    weakWrapper = wrapper;
    [manager setLatestConnectionWrapperForTesting:wrapper];
    wrapper = nil;

    // First retrieval consumes and returns the stored wrapper.
    GNCWiFiAwareConnectionWrapper* retrieved = [manager getLatestConnectionWrapper];
    XCTAssertNotNil(retrieved);

    // Second retrieval must return nil so AwareManager does not retain closed connections.
    XCTAssertNil([manager getLatestConnectionWrapper]);
    retrieved = nil;
  }

  XCTAssertNil(weakWrapper,
               @"AwareManager must not retain connection wrapper after getLatestConnectionWrapper");
}

#endif  // TARGET_OS_IOS && !defined(GITHUB_BUILD)

@end

#pragma clang diagnostic pop
