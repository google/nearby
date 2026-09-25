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

import NearbyConnections
import NearbyCoreAdapter
import XCTest

final class ConsumerTests: XCTestCase {
  func testSwiftConnectionConfiguration() {
    let manager = ConnectionManager(serviceID: "com.google.nearby.cocoapods", strategy: .star)
    XCTAssertEqual(manager.serviceID, "com.google.nearby.cocoapods")
    XCTAssertEqual(manager.strategy, NearbyConnections.Strategy.star)
  }

  func testNativeBytesPayload() {
    let data = Data([0, 127, 128, 255])
    let payload = GNCBytesPayload(data: data, identifier: 42)
    XCTAssertEqual(payload.data, data)
    XCTAssertEqual(payload.identifier, 42)
  }
}
