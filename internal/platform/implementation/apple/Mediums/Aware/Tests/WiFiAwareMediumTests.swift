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

import Foundation
import XCTest
import os

@testable import third_party_nearby_internal_platform_implementation_apple_Mediums_Aware_WiFiAwareMedium

@available(iOS 26.0, *)
final class WiFiAwareMediumTests: XCTestCase {
  override func setUp() {
    super.setUp()
    UserDefaults.standard.removeObject(forKey: PairedPeerRegistry.defaultsKey)
  }

  override func tearDown() {
    UserDefaults.standard.removeObject(forKey: PairedPeerRegistry.defaultsKey)
    super.tearDown()
  }

  // MARK: - isTerminalWiFiAwareDataPathError Tests

  func testIsTerminalWiFiAwareDataPathError_directNSErrorCode() {
    let error = NSError(domain: NSPOSIXErrorDomain, code: kWiFiAwareDataPathError)
    XCTAssertTrue(isTerminalWiFiAwareDataPathError(error))
  }

  func testIsTerminalWiFiAwareDataPathError_underlyingNSErrorCode() {
    let underlying = NSError(domain: NSPOSIXErrorDomain, code: kWiFiAwareDataPathError)
    let wrapped = NSError(
      domain: "TestDomain",
      code: -1,
      userInfo: [NSUnderlyingErrorKey: underlying]
    )
    XCTAssertTrue(isTerminalWiFiAwareDataPathError(wrapped))
  }

  func testIsTerminalWiFiAwareDataPathError_textualFallback() {
    struct CustomTextError: Error, CustomStringConvertible {
      var description: String { "POSIXErrorCode(rawValue: \(kWiFiAwareDataPathError))" }
    }
    XCTAssertTrue(isTerminalWiFiAwareDataPathError(CustomTextError()))
  }

  func testIsTerminalWiFiAwareDataPathError_returnsFalseForOtherErrors() {
    let nonTerminalError = NSError(domain: NSPOSIXErrorDomain, code: -11992)
    XCTAssertFalse(isTerminalWiFiAwareDataPathError(nonTerminalError))
  }

  // MARK: - PairedPeerRegistry Persistence Tests

  func testPairedPeerRegistry_saveLoadAndForget() {
    let peerA = "8c983c575d361dbb"
    let peerB = "1122334455667788"
    let entryA = PairedPeerRegistry.Entry(deviceId: 2, fingerprint: "Pixel 11 Pro XL")
    let entryB = PairedPeerRegistry.Entry(deviceId: 3, fingerprint: "Pixel 10 Pro")

    PairedPeerRegistry.save([peerA: entryA, peerB: entryB])

    let loaded = PairedPeerRegistry.load()
    XCTAssertEqual(loaded[peerA], entryA)
    XCTAssertEqual(loaded[peerB], entryB)

    PairedPeerRegistry.forget(peerId: peerA)
    let afterForget = PairedPeerRegistry.load()
    XCTAssertNil(afterForget[peerA])
    XCTAssertEqual(afterForget[peerB], entryB)
  }

  func testPairedPeerRegistry_dropsMalformedLegacyEntriesWithoutSeparator() {
    UserDefaults.standard.set(
      [
        "legacyPeer": "2",
        "validPeer": "2|Pixel 11 Pro XL",
      ],
      forKey: PairedPeerRegistry.defaultsKey
    )

    let loaded = PairedPeerRegistry.load()
    XCTAssertNil(loaded["legacyPeer"])
    XCTAssertEqual(
      loaded["validPeer"],
      PairedPeerRegistry.Entry(deviceId: 2, fingerprint: "Pixel 11 Pro XL")
    )
  }

  // MARK: - AwareManager.browse Retry & Registry Preservation Tests

  private func runBrowse(
    manager: AwareManager,
    port: Int,
    peerId: String
  ) async -> NSError? {
    await withCheckedContinuation { continuation in
      manager.browse(port: port, peerId: peerId) { error in
        continuation.resume(returning: error)
      }
    }
  }

  func testBrowse_retriesOnTerminalDataPathErrorAndSucceedsWithoutEvictingPeer() async {
    let peerId = "8c983c575d361dbb"
    let seededEntry = PairedPeerRegistry.Entry(deviceId: 2, fingerprint: "Pixel 11 Pro XL")
    PairedPeerRegistry.save([peerId: seededEntry])

    let manager = AwareManager()
    manager.retryDelayNanoseconds = 1_000_000  // 1ms for fast unit tests
    let attemptCounter = OSAllocatedUnfairLock(initialState: 0)

    manager.browseHookForTesting = { port, _, requestedPeerId in
      XCTAssertEqual(port, 33349)
      XCTAssertEqual(requestedPeerId, peerId)
      let attempt = attemptCounter.withLock { count -> Int in
        count += 1
        return count
      }
      if attempt < 3 {
        throw NSError(
          domain: "InternalConnectionWrapper",
          code: -1,
          userInfo: [NSLocalizedDescriptionKey: kTerminalDataPathFailureReason]
        )
      }
      return WiFiAwareConnectionWrapper(asListener: false)
    }

    let browseError = await runBrowse(manager: manager, port: 33349, peerId: peerId)
    XCTAssertNil(browseError)

    XCTAssertEqual(attemptCounter.withLock { $0 }, 3)
    XCTAssertNotNil(manager.getLatestConnectionWrapper())
    // Verify getLatestConnectionWrapper consumes the reference so it is not retained across sessions.
    XCTAssertNil(manager.getLatestConnectionWrapper())
    // Verify the pairing record in PairedPeerRegistry was NOT evicted by transient -11987 errors.
    XCTAssertEqual(PairedPeerRegistry.load()[peerId], seededEntry)
  }

  func testBrowse_stopsAfterMaxAttemptsOnRepeatedTerminalDataPathErrorAndPreservesPeer() async {
    let peerId = "8c983c575d361dbb"
    let seededEntry = PairedPeerRegistry.Entry(deviceId: 2, fingerprint: "Pixel 11 Pro XL")
    PairedPeerRegistry.save([peerId: seededEntry])

    let manager = AwareManager()
    manager.retryDelayNanoseconds = 1_000_000  // 1ms for fast unit tests
    let attemptCounter = OSAllocatedUnfairLock(initialState: 0)

    manager.browseHookForTesting = { _, _, _ in
      attemptCounter.withLock { $0 += 1 }
      throw NSError(domain: NSPOSIXErrorDomain, code: kWiFiAwareDataPathError)
    }

    let browseError = await runBrowse(manager: manager, port: 33349, peerId: peerId)
    XCTAssertNotNil(browseError)
    XCTAssertEqual(browseError?.code, kWiFiAwareDataPathError)

    XCTAssertEqual(attemptCounter.withLock { $0 }, kMaxDataPathBrowseAttempts)
    XCTAssertNil(manager.getLatestConnectionWrapper())
    // Even if all data-path attempts fail, the valid pairing record must remain intact.
    XCTAssertEqual(PairedPeerRegistry.load()[peerId], seededEntry)
  }

  func testBrowse_nonTerminalErrorDoesNotLoopDataPathRetries() async {
    let manager = AwareManager()
    manager.retryDelayNanoseconds = 1_000_000
    let attemptCounter = OSAllocatedUnfairLock(initialState: 0)

    manager.browseHookForTesting = { _, _, _ in
      attemptCounter.withLock { $0 += 1 }
      throw NSError(domain: NSPOSIXErrorDomain, code: -11992)
    }

    let browseError = await runBrowse(manager: manager, port: 33349, peerId: "")
    XCTAssertNotNil(browseError)
    XCTAssertEqual(browseError?.code, -11992)

    XCTAssertEqual(attemptCounter.withLock { $0 }, 1)
  }

  // MARK: - WiFiAwareConnectionWrapper Listener Tests

  func testListenerWrapper_buffersAndAcceptsIncomingConnection() async {
    let closedFlag = OSAllocatedUnfairLock(initialState: false)
    let listener = WiFiAwareConnectionWrapper(asListener: true) {
      closedFlag.withLock { $0 = true }
    }
    let incoming = WiFiAwareConnectionWrapper(asListener: false)

    listener.addIncomingConnection(incoming)
    let accepted = await listener.acceptConnection()
    XCTAssertTrue(accepted === incoming)

    listener.close()
    try? await Task.sleep(nanoseconds: 50_000_000)
    let afterClose = await listener.acceptConnection()
    XCTAssertNil(afterClose)
    XCTAssertTrue(closedFlag.withLock { $0 })
  }
}
