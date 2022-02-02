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

#import <XCTest/XCTest.h>

#import "internal/platform/byte_array.h"
#import "internal/platform/exception.h"
#import "internal/platform/implementation/ios/Source/Platform/input_file.h"

using ::location::nearby::ios::InputFile;
using ::location::nearby::ByteArray;
using ::location::nearby::ExceptionOr;

@interface GNCInputFileTest : XCTestCase
@end

@implementation GNCInputFileTest

// TODO(b/169292092): Find more tools (e.g. file/util/TempPath) to test fake file.
- (void)testNonExistentPath {
  std::string cc_path("/foo/bar/test.ext");
  NSString* objcPath = [NSString stringWithUTF8String:cc_path.c_str()];
  NSURL *testURL = [NSURL URLWithString:objcPath];

  auto input_file = std::make_unique<InputFile>(testURL);
  XCTAssert(input_file != nullptr);

  XCTAssertEqual(input_file->GetTotalSize(), 0);
  ExceptionOr<ByteArray> read_result = input_file->Read(3);
  XCTAssertFalse(read_result.ok());
}

- (void)testGetFilePath {
  std::string cc_path("/foo/bar/test.ext");
  NSString* objcPath = [NSString stringWithUTF8String:cc_path.c_str()];
  NSURL *testURL = [NSURL URLWithString:objcPath];

  auto input_file = std::make_unique<InputFile>(testURL);
  XCTAssert(input_file != nullptr);

  std::string input_file_path = input_file->GetFilePath();
  XCTAssertTrue([objcPath isEqualToString:[NSString stringWithUTF8String:input_file_path.c_str()]]);
}

@end
