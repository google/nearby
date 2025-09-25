// Copyright 2023 Google LLC
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

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

// A custom NSURLProtocol to intercept and fake network requests for testing.
@interface GNCFakeURLProtocol : NSURLProtocol
+ (void)setFakeResponseData:(NSData *)data;
+ (void)setFakeStatusCode:(NSInteger)statusCode;
+ (void)setFakeError:(NSError *)error;
+ (void)clearFakes;
@end

static NSData *gFakeResponseData = nil;
static NSInteger gFakeStatusCode = 0;
static NSError *gFakeError = nil;

@implementation GNCFakeURLProtocol

+ (void)setFakeResponseData:(NSData *)data {
  gFakeResponseData = [data copy];
}

+ (void)setFakeStatusCode:(NSInteger)statusCode {
  gFakeStatusCode = statusCode;
}

+ (void)setFakeError:(NSError *)error {
  gFakeError = [error copy];
}

+ (void)clearFakes {
  gFakeResponseData = nil;
  gFakeStatusCode = 0;
  gFakeError = nil;
}

+ (BOOL)canInitWithRequest:(NSURLRequest *)request {
  // Intercept all HTTP/HTTPS requests.
  return
      [request.URL.scheme isEqualToString:@"http"] || [request.URL.scheme isEqualToString:@"https"];
}

+ (NSURLRequest *)canonicalRequestForRequest:(NSURLRequest *)request {
  return request;
}

- (void)startLoading {
  id<NSURLProtocolClient> client = self.client;
  NSURLRequest *request = self.request;

  if (gFakeError) {
    [client URLProtocol:self didFailWithError:gFakeError];
    return;
  }

  NSHTTPURLResponse *response =
      [[NSHTTPURLResponse alloc] initWithURL:request.URL
                                  statusCode:gFakeStatusCode
                                 HTTPVersion:@"HTTP/1.1"
                                headerFields:@{@"Content-Type" : @"text/plain"}];

  [client URLProtocol:self
      didReceiveResponse:response
      cacheStoragePolicy:NSURLCacheStorageNotAllowed];
  if (gFakeResponseData) {
    [client URLProtocol:self didLoadData:gFakeResponseData];
  }
  [client URLProtocolDidFinishLoading:self];
}

- (void)stopLoading {
  // Required to be implemented, but nothing to do.
}
@end

void GNCEnsureFileAtPath(std::string path) {
  [NSFileManager.defaultManager
            createDirectoryAtPath:[@(path.c_str()) stringByDeletingLastPathComponent]
      withIntermediateDirectories:YES
                       attributes:nil
                            error:nil];
  [[NSData data] writeToFile:@(path.c_str()) options:0 error:nil];
}

@interface GNCPlatformTest : XCTestCase
@end

@implementation GNCPlatformTest

- (void)setUp {
  [super setUp];
  // Register the fake protocol before each test.
  [NSURLProtocol registerClass:[GNCFakeURLProtocol class]];
}

- (void)tearDown {
  // Unregister the fake protocol and clean up fake data after each test.
  [NSURLProtocol unregisterClass:[GNCFakeURLProtocol class]];
  [GNCFakeURLProtocol clearFakes];
  [super tearDown];
}

- (void)testSendRequest_Success {
  // Arrange: Set up a fake successful response.
  std::string responseBody = "{\"status\": \"ok\"}";
  [GNCFakeURLProtocol setFakeResponseData:[NSData dataWithBytes:responseBody.c_str()
                                                         length:responseBody.length()]];
  [GNCFakeURLProtocol setFakeStatusCode:200];

  nearby::api::ImplementationPlatform platform;
  nearby::api::WebRequest request;
  request.url = "https://example.com/api/status";

  // Act: Call the method under test.
  absl::StatusOr<nearby::api::WebResponse> response = platform.SendRequest(request);

  // Assert: Verify the response is successful and contains the fake data.
  XCTAssertTrue(response.ok());
  XCTAssertEqual(response->status_code, 200);
  XCTAssertEqual(response->body, responseBody);
}

- (void)testSendRequest_NetworkError {
  // Arrange: Set up a fake network error.
  [GNCFakeURLProtocol setFakeError:[NSError errorWithDomain:NSURLErrorDomain
                                                       code:NSURLErrorNotConnectedToInternet
                                                   userInfo:nil]];

  nearby::api::ImplementationPlatform platform;
  nearby::api::WebRequest request;
  request.url = "https://example.com/api/status";

  // Act: Call the method under test.
  absl::StatusOr<nearby::api::WebResponse> response = platform.SendRequest(request);

  // Assert: Verify that the call resulted in an error status.
  XCTAssertFalse(response.ok());
  XCTAssertEqual(response.status().code(), absl::StatusCode::kFailedPrecondition);
}

- (void)testGetCustomSavePath {
  NSString *expected = [NSURL fileURLWithPath:@"a/b/c.d"].path;
  std::string actual = nearby::api::ImplementationPlatform::GetCustomSavePath("a/b", "c.d");
  XCTAssertEqualObjects(@(actual.c_str()), expected);
}

- (void)testGetCustomSavePathWithIllegalCharacters {
  NSString *expected = [NSURL fileURLWithPath:@"a/b/Fi?le*: Name.ext"].path;
  std::string actual =
      nearby::api::ImplementationPlatform::GetCustomSavePath("a/b", "Fi?le*/ Name.ext");
  XCTAssertEqualObjects(@(actual.c_str()), expected);
}

- (void)testGetCustomSavePathWithPathEscapingCharacters {
  NSString *expected = [NSURL fileURLWithPath:@"a/b/..:c:..:d.e"].path;
  std::string actual =
      nearby::api::ImplementationPlatform::GetCustomSavePath("a/../../b", "../c/../d.e");
  XCTAssertEqualObjects(@(actual.c_str()), expected);
}

- (void)testGetCustomSavePathDuplicateNames {
  NSString *expected1 = [NSURL fileURLWithPath:@"a/b/cat.jpg"].path;
  NSString *expected2 = [NSURL fileURLWithPath:@"a/b/cat 2.jpg"].path;
  NSString *expected3 = [NSURL fileURLWithPath:@"a/b/cat 3.jpg"].path;

  std::string actual1 = nearby::api::ImplementationPlatform::GetCustomSavePath("a/b", "cat.jpg");
  GNCEnsureFileAtPath(actual1);

  std::string actual2 = nearby::api::ImplementationPlatform::GetCustomSavePath("a/b", "cat.jpg");
  GNCEnsureFileAtPath(actual2);

  std::string actual3 = nearby::api::ImplementationPlatform::GetCustomSavePath("a/b", "cat.jpg");

  // Cleanup created files.
  [NSFileManager.defaultManager removeItemAtPath:@(actual1.c_str()) error:nil];
  [NSFileManager.defaultManager removeItemAtPath:@(actual2.c_str()) error:nil];

  XCTAssertEqualObjects(@(actual1.c_str()), expected1);
  XCTAssertEqualObjects(@(actual2.c_str()), expected2);
  XCTAssertEqualObjects(@(actual3.c_str()), expected3);
}

- (void)testGetDownloadPath {
  NSString *expected =
      [[NSURL fileURLWithPath:NSTemporaryDirectory()] URLByAppendingPathComponent:@"a/b/c.d"].path;
  std::string actual = nearby::api::ImplementationPlatform::GetDownloadPath("a/b", "c.d");
  XCTAssertEqualObjects(@(actual.c_str()), expected);
}

- (void)testGetCurrentOS {
  XCTAssertEqual(nearby::api::ImplementationPlatform::GetCurrentOS(), nearby::api::OSName::kApple);
}

- (void)testCreateAtomicBoolean {
  auto atomic_boolean = nearby::api::ImplementationPlatform::CreateAtomicBoolean(true);
  XCTAssertNotEqual(atomic_boolean.get(), nullptr);
  XCTAssertTrue(atomic_boolean->Get());
}

- (void)testCreateAtomicUint32 {
  auto atomic_uint32 = nearby::api::ImplementationPlatform::CreateAtomicUint32(123);
  XCTAssertNotEqual(atomic_uint32.get(), nullptr);
  XCTAssertEqual(atomic_uint32->Get(), 123);
}

- (void)testCreateCountDownLatch {
  auto count_down_latch = nearby::api::ImplementationPlatform::CreateCountDownLatch(1);
  XCTAssertNotEqual(count_down_latch.get(), nullptr);
}

- (void)testCreateMutex {
  auto mutex = nearby::api::ImplementationPlatform::CreateMutex(nearby::api::Mutex::Mode::kRegular);
  XCTAssertNotEqual(mutex.get(), nullptr);
}

- (void)testCreateConditionVariable {
  auto mutex = nearby::api::ImplementationPlatform::CreateMutex(nearby::api::Mutex::Mode::kRegular);
  auto condition_variable =
      nearby::api::ImplementationPlatform::CreateConditionVariable(mutex.get());
  XCTAssertNotEqual(condition_variable.get(), nullptr);
}

- (void)testCreateBluetoothAdapter {
  auto bluetooth_adapter = nearby::api::ImplementationPlatform::CreateBluetoothAdapter();
  XCTAssertNotEqual(bluetooth_adapter.get(), nullptr);
}

- (void)testCreateOutputFile {
  NSString *path = [NSTemporaryDirectory() stringByAppendingPathComponent:@"test.txt"];

  auto output_file = nearby::api::ImplementationPlatform::CreateOutputFile(path.UTF8String);

  XCTAssertNotEqual(output_file.get(), nullptr);
  XCTAssertTrue([NSFileManager.defaultManager fileExistsAtPath:path]);

  [NSFileManager.defaultManager removeItemAtPath:path error:nil];
}

- (void)testCreateInputFile {
  NSString *path = [NSTemporaryDirectory() stringByAppendingPathComponent:@"test.txt"];
  NSData *data = [@"test data" dataUsingEncoding:NSUTF8StringEncoding];
  [data writeToFile:path atomically:YES];

  auto input_file =
      nearby::api::ImplementationPlatform::CreateInputFile(path.UTF8String, data.length);

  XCTAssertNotEqual(input_file.get(), nullptr);
  XCTAssertEqual(input_file->GetTotalSize(), data.length);
  XCTAssertEqualObjects(@(input_file->GetFilePath().c_str()), path);

  [NSFileManager.defaultManager removeItemAtPath:path error:nil];
}

- (void)testCreateWifiMedium {
  auto wifi_medium = nearby::api::ImplementationPlatform::CreateWifiMedium();
  XCTAssertNotEqual(wifi_medium.get(), nullptr);
}

- (void)testCreateWifiLanMedium {
  auto wifi_lan_medium = nearby::api::ImplementationPlatform::CreateWifiLanMedium();
  XCTAssertNotEqual(wifi_lan_medium.get(), nullptr);
}

- (void)testCreateAwdlMedium {
  auto awdl_medium = nearby::api::ImplementationPlatform::CreateAwdlMedium();
  XCTAssertNotEqual(awdl_medium.get(), nullptr);
}

- (void)testCreateWifiHotspotMedium {
  auto wifi_hotspot_medium = nearby::api::ImplementationPlatform::CreateWifiHotspotMedium();
  XCTAssertNotEqual(wifi_hotspot_medium.get(), nullptr);
}

- (void)testCreateTimer {
  auto timer = nearby::api::ImplementationPlatform::CreateTimer();
  XCTAssertNotEqual(timer.get(), nullptr);
}

- (void)testCreateDeviceInfo {
  auto device_info = nearby::api::ImplementationPlatform::CreateDeviceInfo();
  XCTAssertNotEqual(device_info.get(), nullptr);
}

- (void)testCreatePreferencesManager {
  auto preferences_manager = nearby::api::ImplementationPlatform::CreatePreferencesManager("test");
  XCTAssertNotEqual(preferences_manager.get(), nullptr);
}

- (void)testCreateBluetoothClassicMedium {
  auto bluetooth_adapter = nearby::api::ImplementationPlatform::CreateBluetoothAdapter();
  auto bluetooth_classic_medium =
      nearby::api::ImplementationPlatform::CreateBluetoothClassicMedium(*bluetooth_adapter);
  XCTAssertEqual(bluetooth_classic_medium.get(), nullptr);
}

- (void)testCreateBleV2Medium {
  auto bluetooth_adapter = nearby::api::ImplementationPlatform::CreateBluetoothAdapter();
  auto ble_medium = nearby::api::ImplementationPlatform::CreateBleV2Medium(*bluetooth_adapter);
  XCTAssertNotEqual(ble_medium.get(), nullptr);
}

- (void)testCreateServerSyncMedium {
  auto server_sync_medium = nearby::api::ImplementationPlatform::CreateServerSyncMedium();
  XCTAssertEqual(server_sync_medium.get(), nullptr);
}

- (void)testCreateWifiDirectMedium {
  auto wifi_direct_medium = nearby::api::ImplementationPlatform::CreateWifiDirectMedium();
  XCTAssertEqual(wifi_direct_medium.get(), nullptr);
}

- (void)testCreateWebRtcMedium {
  auto webrtc_medium = nearby::api::ImplementationPlatform::CreateWebRtcMedium();
  XCTAssertEqual(webrtc_medium.get(), nullptr);
}

- (void)testCreateInputFileWithPayloadID {
  auto input_file = nearby::api::ImplementationPlatform::CreateInputFile(1234, 0);
  XCTAssertEqual(input_file.get(), nullptr);
}

- (void)testCreateOutputFileWithPayloadID {
  auto output_file = nearby::api::ImplementationPlatform::CreateOutputFile(1234);
  XCTAssertEqual(output_file.get(), nullptr);
}

@end
