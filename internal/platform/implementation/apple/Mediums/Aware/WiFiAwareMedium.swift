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

import DeviceDiscoveryUI  // Private
import Foundation
import Network
import WiFiAware
import os

#if canImport(UIKit)
  import UIKit  // UIHostingController is in UIKit
#endif

struct LocalLogger {
  private let osLogger: os.Logger

  init(component: String) {
    self.osLogger = os.Logger(subsystem: "com.google.nearby", category: "WiFiAware.\(component)")
  }

  func info(_ message: String) {
    osLogger.info("\(message)")
  }

  func warning(_ message: String) {
    osLogger.warning("\(message)")
  }

  func error(_ message: String) {
    osLogger.error("\(message)")
  }
}
typealias Logger = LocalLogger

/// Builds the error thrown when a Wi-Fi Aware service is not declared in the app's Info.plist.
private func missingWiFiAwareServiceError(_ name: String, role: String) -> NSError {
  return NSError(
    domain: "WiFiAwareMedium", code: -1,
    userInfo: [
      NSLocalizedDescriptionKey:
        "Missing Wi-Fi Aware \(role) service \(name); declare it under WiFiAwareServices in the "
        + "app's Info.plist"
    ])
}

@available(iOS 26.0, *)
extension WAPublishableService {
  /// Returns the publishable service called `name`, as declared in the app's Info.plist.
  public static func named(_ name: String) throws -> WAPublishableService {
    guard let service = allServices[name] else {
      throw missingWiFiAwareServiceError(name, role: "publishable")
    }
    return service
  }
}

@available(iOS 26.0, *)
extension WASubscribableService {
  /// Returns the subscribable service called `name`, as declared in the app's Info.plist.
  public static func named(_ name: String) throws -> WASubscribableService {
    guard let service = allServices[name] else {
      throw missingWiFiAwareServiceError(name, role: "subscribable")
    }
    return service
  }
}

@available(iOS 26.0, *)
extension WAAccessCategory {
  var serviceClass: NWParameters.ServiceClass {
    switch self {
    case .bestEffort:
      return .bestEffort
    case .background:
      return .background
    case .interactiveVideo:
      return .interactiveVideo
    case .interactiveVoice:
      return .interactiveVoice
    @unknown default:
      return .bestEffort
    }
  }
}

@available(iOS 26.0, *)
extension WAPairedDevice {
  var displayName: String {
    let displayName = self.name ?? self.pairingInfo?.pairingName ?? ""
    return "\(displayName) (\(self.pairingInfo?.vendorName ?? ""))"
  }
}

@available(iOS 26.0, *)
func logPairedDevices() async -> Int {
  let logger = Logger(component: "WiFiAware")
  do {
    guard let devices = try await WAPairedDevice.allDevices.current() else {
      logger.info("logPairedDevices: Failed to get paired devices.")
      return 0
    }
    logger.info("logPairedDevices: \(devices.count) devices.")
    for (id, device) in devices {
      let pairingName = device.pairingInfo?.pairingName ?? "nil"
      let vendorName = device.pairingInfo?.vendorName ?? "nil"
      let modelName = device.pairingInfo?.modelName ?? "nil"
      logger.info(
        "Paired Device: id=\(id), displayName=\(device.displayName), pairingName=\(pairingName), vendor=\(vendorName), model=\(modelName)"
      )
    }
    return devices.count
  } catch {
    logger.error("logPairedDevices: exception: \(error)")
    return 0
  }
}

/// Remembers which `WAPairedDevice` belongs to which remote peer.
///
/// A peer identifies itself with a stable string in its bandwidth-upgrade frame
/// (`WifiAwareR4Credentials.advertised_name`). Everything else it sends is per-session — endpoint
/// IDs are regenerated from `SecureRandom` and rotate, service IDs are per transfer — so this
/// string is the only durable handle we have on "which device is this".
///
/// Without it the only question we can answer is *"is anything at all paired?"*, which is the
/// wrong question and the direct cause of the second-device failure: with one Android already
/// paired, a request from a second, unpaired Android answered "yes", the pairing UI was skipped,
/// and the browse that followed was filtered by `wifip2pd` to the first device's allow-list. The
/// second device was then dropped during discovery with no error surfaced anywhere.
///
/// ## Why an entry is more than a `WAPairedDevice.ID`
///
/// `WAPairedDevice.ID` is a small integer that the system **reuses**: unpair everything, pair a
/// different device, and it is handed the same `1` the previous one had. A persisted entry is
/// therefore not safe to trust just because *something* still holds its ID — that produced the
/// exact bug this guards against, where a stale entry for one phone silently aliased onto a
/// different phone's new pairing and we skipped the pairing UI for a device we had never paired
/// with.
///
/// Nothing in `WAPairedDevice` is durable and unique (`pairingInfo` is only name/vendor/model; the
/// stable `PairingKeyStoreID` that `wifip2pd` keeps internally is not exposed), so entries are
/// defended three ways instead:
///
///  1. every read prunes the *whole* registry against the live store, not just the entry being
///     looked up — an entry that is never queried can no longer rot unnoticed;
///  2. an ID maps to exactly one peer, so recording a new pairing evicts any other peer claiming
///     that ID;
///  3. entries carry a fingerprint of the device they were bound to and stop resolving when the
///     device behind the ID no longer matches it.
///
/// Any one of the three is enough to catch plain ID reuse. Together they also cover reuse between
/// two devices that report identical names, which the fingerprint alone cannot see. Failure is
/// always towards showing the pairing UI again, which costs the user a tap and never strands them.
@available(iOS 26.0, *)
enum PairedPeerRegistry {
  private static let defaultsKey = "QSAwarePairedPeerIdentifiers"
  private static let logger = Logger(component: "PairedPeerRegistry")

  /// A paired device, plus enough of its identity to notice when its ID gets handed to someone
  /// else.
  private struct Entry {
    var deviceId: WAPairedDevice.ID
    var fingerprint: String
  }

  /// Everything `WAPairedDevice` exposes that describes the device rather than the session.
  ///
  /// This does not uniquely identify a device — two of the same model are indistinguishable — so
  /// it is a corroborating check, never the only one.
  private static func fingerprint(of device: WAPairedDevice) -> String {
    let info = device.pairingInfo
    return [
      device.name ?? "",
      info?.pairingName ?? "",
      info?.vendorName ?? "",
      info?.modelName ?? "",
    ].joined(separator: "\u{1}")
  }

  /// `UserDefaults` cannot store `UInt64` in a dictionary directly, so entries are encoded as
  /// `"<id>|<fingerprint>"`.
  private static func load() -> [String: Entry] {
    guard let raw = UserDefaults.standard.dictionary(forKey: defaultsKey) as? [String: String]
    else {
      return [:]
    }
    return raw.compactMapValues { value in
      // Entries written before fingerprinting have no separator. They are exactly the ones that
      // could already be aliased onto the wrong device, so they are dropped rather than migrated;
      // the cost is one extra pairing prompt per peer.
      let parts = value.split(separator: "|", maxSplits: 1, omittingEmptySubsequences: false)
      guard parts.count == 2, let deviceId = UInt64(parts[0]) else { return nil }
      return Entry(deviceId: deviceId, fingerprint: String(parts[1]))
    }
  }

  private static func save(_ map: [String: Entry]) {
    UserDefaults.standard.set(
      map.mapValues { "\($0.deviceId)|\($0.fingerprint)" }, forKey: defaultsKey)
  }

  /// Drops every entry that no longer describes a device in `devices`, and persists the result.
  ///
  /// Pruning the whole registry rather than just the entry we came for is the point: the entry
  /// that goes stale is usually not the one being looked up, and it stays wrong until something
  /// forces a re-check.
  private static func pruned(against devices: WAPairedDevice.Devices) -> [String: Entry] {
    let map = load()
    var kept: [String: Entry] = [:]
    for (peerId, entry) in map {
      guard let device = devices[entry.deviceId] else {
        logger.info("pairing \(entry.deviceId) for peer \(peerId) is gone, forgetting it")
        continue
      }
      guard fingerprint(of: device) == entry.fingerprint else {
        logger.info(
          "paired device \(entry.deviceId) is now \(device.displayName), not what peer \(peerId) "
            + "was bound to, forgetting it")
        continue
      }
      kept[peerId] = entry
    }
    if kept.count != map.count {
      save(kept)
    }
    return kept
  }

  /// Returns the device currently paired with `peerId`, or nil if we have never paired with it or
  /// the pairing has since been removed, replaced, or reassigned.
  static func pairedDevice(forPeerId peerId: String) async -> WAPairedDevice? {
    guard !peerId.isEmpty else { return nil }
    do {
      let devices = try await WAPairedDevice.allDevices.current() ?? [:]
      guard let entry = pruned(against: devices)[peerId] else {
        logger.info("no pairing recorded for peer \(peerId)")
        return nil
      }
      guard let device = devices[entry.deviceId] else { return nil }
      logger.info(
        "peer \(peerId) resolves to paired device \(entry.deviceId) (\(device.displayName))")
      return device
    } catch {
      logger.error("failed to read paired devices: \(error)")
      return nil
    }
  }

  /// Binds `device` to `peerId`, taking the pairing away from any other peer that claimed it.
  static func remember(peerId: String, device: WAPairedDevice) {
    guard !peerId.isEmpty else { return }
    var map = load()
    // A pairing belongs to exactly one peer. If another peer still points at this ID its entry
    // predates the ID being reassigned, so it is stale by definition.
    for (otherPeerId, entry) in map where otherPeerId != peerId && entry.deviceId == device.id {
      logger.info(
        "paired device \(device.id) was recorded for peer \(otherPeerId); it now belongs to "
          + "\(peerId), dropping the old entry")
      map.removeValue(forKey: otherPeerId)
    }
    map[peerId] = Entry(deviceId: device.id, fingerprint: fingerprint(of: device))
    save(map)
    logger.info("recorded peer \(peerId) -> paired device \(device.id) (\(device.displayName))")
  }

  static func forget(peerId: String) {
    var map = load()
    map.removeValue(forKey: peerId)
    save(map)
  }

  /// IDs of every device currently in the paired store. Used to tell which entry a pairing added.
  static func currentDeviceIds() async -> Set<WAPairedDevice.ID> {
    do {
      guard let devices = try await WAPairedDevice.allDevices.current() else { return [] }
      return Set(devices.keys)
    } catch {
      logger.error("failed to snapshot paired devices: \(error)")
      return []
    }
  }
}

// Config constants matching Apple Sample (BuildingPeerToPeerApps / NetworkConfig.swift)
@available(iOS 26.0, *)
let appPerformanceMode: WAPerformanceMode = .realtime

@available(iOS 26.0, *)
let appAccessCategory: WAAccessCategory = .interactiveVideo

@available(iOS 26.0, *)
let appServiceClass: NWParameters.ServiceClass = appAccessCategory.serviceClass

@available(iOS 26.0, *)
typealias WiFiAwareConnection = NetworkConnection<TCP>
typealias WiFiAwareConnectionID = String

@available(iOS 26.0, *)
typealias WiFiAwareConnectionState = (WiFiAwareConnection, WiFiAwareConnection.State)

@available(iOS 26.0, *)
private struct ConnectionInfo: Sendable {
  let receiverTask: Task<Void, Error>
  let stateUpdateTask: Task<Void, Error>
  var remoteDevice: WAPairedDevice?
}

// MARK: - Internal Connection Actor (Data Pump & Backpressure)

@available(iOS 26.0, *)
actor InternalConnectionWrapper {
  private let connection: WiFiAwareConnection
  private let onClose: @Sendable () -> Void
  private var buffer = Data()
  private var continuation: CheckedContinuation<Void, Never>?
  private var readyContinuation: CheckedContinuation<Void, Error>?
  private var isClosed = false
  private var isReady = false
  private let logger = Logger(component: "InternalConnectionWrapper")

  // Bounded producer-consumer queue state
  private var writeQueue: [Data] = []
  private var writeQueueBytes = 0
  private var senderContinuation: CheckedContinuation<Void, Never>?
  private var writeContinuation: CheckedContinuation<Void, Never>?
  private var senderTask: Task<Void, Never>?
  private let maxQueueBytes = 256 * 1024  // 256 KB High-Water Mark
  private let lowWaterQueueBytes = 64 * 1024  // 64 KB Low-Water Mark

  init(connection: WiFiAwareConnection, onClose: @escaping @Sendable () -> Void = {}) {
    self.connection = connection
    self.onClose = onClose
  }

  func startSenderLoop() {
    guard senderTask == nil else { return }
    senderTask = Task(priority: .userInitiated) { [weak self] in
      await self?.runSenderLoop()
    }
  }

  func appendIncomingData(_ data: Data) {
    buffer.append(data)
    continuation?.resume()
    continuation = nil
  }

  /// Closes the wrapper.
  ///
  /// `reason` is surfaced as the `NSLocalizedDescriptionKey` of the error thrown from
  /// `awaitReady()`, so an aborted handshake can be attributed to a specific cause (for example a
  /// terminal NAN Data Path failure) instead of the generic default.
  func signalClosed(reason: String = "Connection closed during handshake") {
    guard !isClosed else { return }
    isClosed = true
    continuation?.resume()
    continuation = nil
    readyContinuation?.resume(
      throwing: NSError(
        domain: "InternalConnectionWrapper", code: -1,
        userInfo: [NSLocalizedDescriptionKey: reason]))
    readyContinuation = nil
    senderContinuation?.resume()
    senderContinuation = nil
    writeContinuation?.resume()
    writeContinuation = nil
    onClose()
  }

  func signalReady() {
    guard !isReady else { return }
    isReady = true
    readyContinuation?.resume(returning: ())
    readyContinuation = nil
  }

  private func waitForReadyContinuation() async throws {
    try await withCheckedThrowingContinuation { (c: CheckedContinuation<Void, Error>) in
      self.readyContinuation = c
    }
  }

  func awaitReady(timeoutSeconds: Double = 15.0) async throws {
    if isReady { return }
    if isClosed {
      throw NSError(
        domain: "InternalConnectionWrapper", code: -1,
        userInfo: [NSLocalizedDescriptionKey: "Connection was closed before handshake completed"])
    }
    try await withThrowingTaskGroup(of: Void.self) { group in
      group.addTask {
        try await self.waitForReadyContinuation()
      }
      group.addTask {
        try await Task.sleep(nanoseconds: UInt64(timeoutSeconds * 1_000_000_000))
        throw NSError(
          domain: "InternalConnectionWrapper", code: -2,
          userInfo: [
            NSLocalizedDescriptionKey:
              "Handshake timed out waiting for ready state (\(timeoutSeconds)s)"
          ])
      }
      try await group.next()
      group.cancelAll()
    }
  }

  func readMaxLength(_ length: Int) async -> Data? {
    guard length > 0 else { return Data() }

    while buffer.isEmpty && !isClosed {
      guard continuation == nil else {
        fatalError("Concurrent reads not supported on this wrapper")
      }

      await withCheckedContinuation { c in
        self.continuation = c
      }
    }

    if buffer.isEmpty && isClosed {
      return nil  // EOF
    }

    let bytesToRead = min(length, buffer.count)
    let data = buffer.prefix(bytesToRead)
    buffer.removeFirst(bytesToRead)
    return data
  }

  func write(_ data: Data) async throws {
    if isClosed {
      throw NSError(
        domain: "InternalConnectionWrapper", code: -1,
        userInfo: [NSLocalizedDescriptionKey: "Connection is closed"])
    }

    let chunkSize = 16 * 1024
    var offset = data.startIndex
    while offset < data.endIndex {
      let nextOffset = min(offset + chunkSize, data.endIndex)
      let chunk = data.subdata(in: offset..<nextOffset)
      writeQueue.append(chunk)
      writeQueueBytes += chunk.count
      offset = nextOffset
    }

    // Wake up sender task if it was suspended waiting for work
    if let pendingSender = senderContinuation {
      self.senderContinuation = nil
      pendingSender.resume()
    }

    // Bounded buffer backpressure: block C++ producer if queue is too full
    if writeQueueBytes >= maxQueueBytes {
      await withCheckedContinuation { c in
        self.writeContinuation = c
      }
      if isClosed {
        throw NSError(
          domain: "InternalConnectionWrapper", code: -1,
          userInfo: [NSLocalizedDescriptionKey: "Connection closed while writing"])
      }
    }
  }

  private func runSenderLoop() async {
    logger.info("runSenderLoop started")
    while !isClosed {
      guard let packet = await getNextPacket() else {
        if isClosed { break }
        continue
      }

      do {
        try await connection.send(packet)
      } catch {
        logger.error("runSenderLoop error sending packet: \(error)")
        self.signalClosed()
        break
      }
    }
    logger.info("runSenderLoop finished")
  }

  private func getNextPacket() async -> Data? {
    if isClosed { return nil }
    if !writeQueue.isEmpty {
      let packet = writeQueue.removeFirst()
      writeQueueBytes -= packet.count

      // Resume producer if queue size dropped below low-water mark
      if writeQueueBytes <= lowWaterQueueBytes, let pendingWrite = writeContinuation {
        self.writeContinuation = nil
        pendingWrite.resume()
      }
      return packet
    }

    // Suspend sender task until new packets are enqueued
    await withCheckedContinuation { c in
      self.senderContinuation = c
    }

    if isClosed { return nil }
    if !writeQueue.isEmpty {
      let packet = writeQueue.removeFirst()
      writeQueueBytes -= packet.count
      if writeQueueBytes <= lowWaterQueueBytes, let pendingWrite = writeContinuation {
        self.writeContinuation = nil
        pendingWrite.resume()
      }
      return packet
    }
    return nil
  }

  func close() {
    senderTask?.cancel()
    isClosed = true
    continuation?.resume()
    continuation = nil
    readyContinuation?.resume(
      throwing: NSError(
        domain: "InternalConnectionWrapper", code: -1,
        userInfo: [NSLocalizedDescriptionKey: "Connection closed"]))
    readyContinuation = nil
    senderContinuation?.resume()
    senderContinuation = nil
    writeContinuation?.resume()
    writeContinuation = nil
  }

  deinit {
    senderTask?.cancel()
  }
}

// MARK: - Internal Listener Actor

@available(iOS 26.0, *)
actor InternalListenerWrapper {
  private let logger = Logger(component: "InternalListenerWrapper")
  private var incomingConnections: [WiFiAwareConnectionWrapper] = []
  private var continuation: CheckedContinuation<WiFiAwareConnectionWrapper?, Never>?
  private var isClosed = false

  func addIncomingConnection(_ wrapper: WiFiAwareConnectionWrapper) {
    logger.info("addIncomingConnection() called with \(wrapper)")
    guard !isClosed else {
      logger.error("addIncomingConnection() failed: listener is closed")
      return
    }
    if let continuation = continuation {
      logger.info("addIncomingConnection() resuming suspended acceptConnection")
      self.continuation = nil
      continuation.resume(returning: wrapper)
    } else {
      logger.info("addIncomingConnection() buffering connection")
      incomingConnections.append(wrapper)
    }
  }

  func acceptConnection() async -> WiFiAwareConnectionWrapper? {
    logger.info("acceptConnection() called")
    guard !isClosed else {
      logger.error("acceptConnection() failed: listener is closed")
      return nil
    }
    if !incomingConnections.isEmpty {
      let conn = incomingConnections.removeFirst()
      logger.info("acceptConnection() returning buffered connection: \(conn)")
      return conn
    }
    guard continuation == nil else {
      fatalError("Concurrent accepts not supported on this wrapper")
    }
    logger.info("acceptConnection() waiting for incoming connection...")
    return await withCheckedContinuation { c in
      self.continuation = c
    }
  }

  func close() {
    logger.info("close() called")
    if !isClosed {
      isClosed = true
      if let continuation = continuation {
        logger.info("close() resuming suspended acceptConnection with nil")
        self.continuation = nil
        continuation.resume(returning: nil)
      }
      incomingConnections.removeAll()
    }
  }
}

// MARK: - Objective-C Connection Wrapper

@available(iOS 26.0, *)
@objc(GNCWiFiAwareConnectionWrapper)
public class WiFiAwareConnectionWrapper: NSObject, @unchecked Sendable {
  private let logger = Logger(component: "WiFiAwareConnectionWrapper")
  let internalWrapper: InternalConnectionWrapper?
  let internalListenerWrapper: InternalListenerWrapper?
  private let onClose: @Sendable () -> Void

  init(connection: WiFiAwareConnection, onClose: @escaping @Sendable () -> Void = {}) {
    let onCloseParam = onClose
    let wrapper = InternalConnectionWrapper(connection: connection, onClose: onCloseParam)
    self.internalWrapper = wrapper
    self.internalListenerWrapper = nil
    self.onClose = onClose
    super.init()
    Task {
      await wrapper.startSenderLoop()
    }
  }

  @objc public init(asListener: Bool, onClose: @escaping @Sendable () -> Void = {}) {
    self.internalWrapper = nil
    self.internalListenerWrapper = asListener ? InternalListenerWrapper() : nil
    self.onClose = onClose
    super.init()
  }

  @objc public func readMaxLength(_ length: Int) async -> Data? {
    guard let wrapper = internalWrapper else {
      logger.error("readMaxLength called on listener wrapper")
      return nil
    }
    return await wrapper.readMaxLength(length)
  }

  @objc public func write(_ data: Data) async throws {
    guard let wrapper = internalWrapper else {
      throw NSError(
        domain: "WiFiAwareConnectionWrapper", code: -1,
        userInfo: [NSLocalizedDescriptionKey: "Cannot write to a listener wrapper"])
    }
    try await wrapper.write(data)
  }

  @objc public func awaitReady() async throws {
    try await internalWrapper?.awaitReady()
  }

  func awaitReady(timeoutSeconds: Double) async throws {
    try await internalWrapper?.awaitReady(timeoutSeconds: timeoutSeconds)
  }

  @objc public func signalReady() {
    Task {
      await internalWrapper?.signalReady()
    }
  }

  func signalClosed(reason: String = "Connection closed during handshake") {
    Task {
      await internalWrapper?.signalClosed(reason: reason)
    }
  }

  func appendIncomingData(_ data: Data) async {
    await internalWrapper?.appendIncomingData(data)
  }

  @objc public func acceptConnection() async -> WiFiAwareConnectionWrapper? {
    guard let wrapper = internalListenerWrapper else {
      logger.error("acceptConnection called on non-listener wrapper")
      return nil
    }
    return await wrapper.acceptConnection()
  }

  func addIncomingConnection(_ wrapper: WiFiAwareConnectionWrapper) {
    Task(priority: .userInitiated) {
      await internalListenerWrapper?.addIncomingConnection(wrapper)
    }
  }

  @objc public func close() {
    Task {
      if internalListenerWrapper != nil {
        await internalListenerWrapper?.close()
        onClose()
      } else {
        await internalWrapper?.close()
        // Give the network stack 500ms to flush any pending packets before deallocating the connection.
        try? await Task.sleep(nanoseconds: 500_000_000)
        onClose()
      }
    }
  }
}

// MARK: - ConnectionManager Actor (Matches Apple Sample Architecture)

/// `NWConnection` error reported when the peer rejects, or the firmware aborts, the NAN Data Path
/// setup. Unlike most `.waiting` errors this one is never recovered from: Apple's `wifip2pd`
/// performs "no data path retries when pairing is enabled", so the connection sits in `.waiting`
/// until an upper-layer timeout fires.
private let kWiFiAwareDataPathError = -11987

/// Human-readable explanation attached to the error that aborts the handshake when
/// `kWiFiAwareDataPathError` is seen.
///
/// The data path is only refused this way when the peer fails to complete NAN pairing. In practice
/// the peer runs *Pair Verify* (resuming a cached pairing) rather than *Pair Setup* (a fresh PIN
/// exchange), and some peers cannot service Pair Verify as the responder. Forgetting the peer on
/// this device clears the cached record and forces the next attempt back onto Pair Setup.
private let kTerminalDataPathFailureReason =
  "Wi-Fi Aware data path refused by peer (\(kWiFiAwareDataPathError)): peer did not complete NAN "
  + "pairing. If the peer only fails when resuming a cached pairing, forget it on this device so "
  + "the next attempt performs a fresh Pair Setup."

/// Returns true when `error` is the terminal Wi-Fi Aware data-path failure (see
/// `kWiFiAwareDataPathError`).
///
/// The concrete error type here is `NWError`, whose bridging to `NSError` is not guaranteed to
/// preserve the underlying numeric code, so the textual form is checked as a fallback.
private func isTerminalWiFiAwareDataPathError(_ error: Error) -> Bool {
  let nsError = error as NSError
  if nsError.code == kWiFiAwareDataPathError {
    return true
  }
  if let underlying = nsError.userInfo[NSUnderlyingErrorKey] as? NSError,
    underlying.code == kWiFiAwareDataPathError
  {
    return true
  }
  return "\(error)".contains("\(kWiFiAwareDataPathError)")
}

@available(iOS 26.0, *)
actor ConnectionManager: Sendable {
  private var connections: [WiFiAwareConnectionID: WiFiAwareConnection] = [:]
  private var wrappers: [WiFiAwareConnectionID: WiFiAwareConnectionWrapper] = [:]
  private var connectionsInfo: [WiFiAwareConnectionID: ConnectionInfo] = [:]
  private let logger = Logger(component: "ConnectionManager")

  func add(_ connection: WiFiAwareConnection, wrapper: WiFiAwareConnectionWrapper) {
    let id = connection.id
    logger.info("Add connection: \(id)")
    connections[id] = connection
    wrappers[id] = wrapper
    connectionsInfo[id] = ConnectionInfo(
      receiverTask: setupReceiver(connection, wrapper: wrapper),
      stateUpdateTask: setupStateUpdateHandler(connection, wrapper: wrapper)
    )
  }

  func setupDirectConnection(
    to host: NWEndpoint.Host,
    port: NWEndpoint.Port,
    interface: NWInterface? = nil
  ) async throws -> WiFiAwareConnectionWrapper {
    let endpoint = NWEndpoint.hostPort(host: host, port: port)
    logger.info("Set up direct host-port connection to \(endpoint)")

    let connection = NetworkConnection(
      to: endpoint,
      using: .parameters {
        TCP().noDelay(true).keepalive(idleTimeInSeconds: 5, count: 5, intervalInSeconds: 1)
      }
      .serviceClass(appServiceClass)
    )

    let id = connection.id
    let wrapper = WiFiAwareConnectionWrapper(connection: connection) { [weak self] in
      guard let self else { return }
      Task {
        await self.stop(id)
      }
    }

    add(connection, wrapper: wrapper)
    return wrapper
  }

  private func extractUnderlyingNWEndpoint(from endpoint: WAEndpoint) -> NWEndpoint? {
    let mirror = Mirror(reflecting: endpoint)
    for child in mirror.children {
      if child.label == "nw", let nwEndpoint = child.value as? NWEndpoint {
        return nwEndpoint
      }
    }
    for child in mirror.children {
      if let nwEndpoint = child.value as? NWEndpoint {
        return nwEndpoint
      }
    }
    return nil
  }

  func setupConnection(
    to endpoint: WAEndpoint,
    nwEndpoint: NWEndpoint? = nil,
    port: Int = 0
  ) async throws -> WiFiAwareConnectionWrapper {
    var targetEndpoint: WAEndpoint = endpoint
    if port > 0, let port16 = UInt16(exactly: port), let nwPort = NWEndpoint.Port(rawValue: port16)
    {
      // NWEndpoint.wifiAware(port:) was introduced in the iOS 26.4 SDK. The Swift compiler version
      // is checked so that this code is only compiled with Xcode 26.4+ toolchains.
      // TODO: edwinwu - Remove the compiler check once Xcode 26.4 is the minimum version.
      #if compiler(>=6.3)
        if #available(iOS 26.4, *) {
          let sourceNWEndpoint = nwEndpoint ?? extractUnderlyingNWEndpoint(from: endpoint)
          if let sourceNWEndpoint = sourceNWEndpoint,
            let portEndpoint = sourceNWEndpoint.wifiAware(port: nwPort)
          {
            targetEndpoint = portEndpoint
            logger.info(
              "Attached port \(port) to WAEndpoint via underlying NWEndpoint (\(sourceNWEndpoint)): \(portEndpoint)"
            )
          } else {
            logger.warning(
              "Failed to attach port \(port) to WAEndpoint (sourceNWEndpoint: \(String(describing: sourceNWEndpoint))), falling back to discovered endpoint: \(endpoint)"
            )
          }
        } else {
          logger.warning(
            "wifiAware(port:) requires iOS 26.4+, falling back to discovered endpoint: \(endpoint)")
        }
      #else
        logger.warning(
          "wifiAware(port:) requires the iOS 26.4 SDK, cannot attach port \(nwPort), falling back to discovered endpoint: \(endpoint)"
        )
      #endif
    }

    let connection = NetworkConnection(
      to: targetEndpoint,
      using: .parameters {
        TCP().noDelay(true).keepalive(idleTimeInSeconds: 5, count: 5, intervalInSeconds: 1)
      }
      .wifiAware { $0.performanceMode = appPerformanceMode }
      .serviceClass(appServiceClass)
    )

    let id = connection.id
    logger.info("Set up connection: \(id) to endpoint: \(targetEndpoint) (targetPort: \(port))")

    let wrapper = WiFiAwareConnectionWrapper(connection: connection) { [weak self] in
      guard let self else { return }
      Task {
        await self.stop(id)
      }
    }

    add(connection, wrapper: wrapper)
    return wrapper
  }

  private func setupStateUpdateHandler(
    _ connection: WiFiAwareConnection, wrapper: WiFiAwareConnectionWrapper
  ) -> Task<Void, Error> {
    let (stream, continuation) = AsyncStream.makeStream(of: WiFiAwareConnectionState.self)

    connection.onStateUpdate { conn, state in
      continuation.yield((conn, state))
    }

    let connId = connection.id
    return Task(priority: .userInitiated) { [weak self] in
      for await (_, state) in stream {
        self?.logger.info("Connection \(connId) onStateUpdate: \(state)")
        switch state {
        case .setup, .preparing:
          break

        case .waiting(let error):
          self?.logger.error("Connection \(connId) is .waiting with error: \(error)")
          // `.waiting` is normally a recoverable state, but the NAN Data Path rejection is not:
          // wifip2pd does not retry a data path while pairing is enabled, so nothing will move
          // this connection forward. Fail fast instead of blocking awaitReady() and then the 40 s
          // ConnectToService timeout, which keeps the transfer on BLE for ~55 s.
          if isTerminalWiFiAwareDataPathError(error) {
            self?.logger.error(
              "Connection \(connId) hit a terminal Wi-Fi Aware data-path error, failing now")
            // A cached pairing record is what makes iOS run Pair Verify instead of Pair Setup, so
            // log it here: it is the single most useful fact when triaging this failure.
            let pairedCount = await logPairedDevices()
            self?.logger.error(
              "Connection \(connId) had \(pairedCount) cached Wi-Fi Aware pairing record(s)")
            wrapper.signalClosed(reason: kTerminalDataPathFailureReason)
            if let self = self {
              await self.stop(connId)
            }
          }

        case .ready:
          self?.logger.info("Connection \(connId) is .ready!")
          wrapper.signalReady()

        case .failed(let error):
          self?.logger.error("Connection \(connId) failed with error: \(error)")
          wrapper.signalClosed()
          if let self = self {
            await self.stop(connId)
          }

        case .cancelled:
          self?.logger.info("Connection \(connId) cancelled")
          wrapper.signalClosed()
          if let self = self {
            await self.stop(connId)
          }

        @unknown default:
          break
        }
      }
    }
  }

  private func setupReceiver(_ connection: WiFiAwareConnection, wrapper: WiFiAwareConnectionWrapper)
    -> Task<Void, Error>
  {
    let connId = connection.id
    return Task(priority: .userInitiated) { [weak self] in
      do {
        while true {
          let message = try await connection.receive(atLeast: 1, atMost: 64 * 1024)
          if !message.content.isEmpty {
            await wrapper.appendIncomingData(message.content)
          }
          if message.metadata.endOfStream {
            self?.logger.info("Connection \(connId) received endOfStream")
            break
          }
        }
      } catch {
        self?.logger.error("Error receiving messages on \(connId): \(error)")
      }
      wrapper.signalClosed()
      if let self = self {
        await self.stop(connId)
      }
    }
  }

  func stop(_ id: WiFiAwareConnectionID) {
    logger.info("Stop connection: \(id)")
    connectionsInfo[id]?.receiverTask.cancel()
    connectionsInfo[id]?.stateUpdateTask.cancel()
    connectionsInfo.removeValue(forKey: id)
    connections.removeValue(forKey: id)
    wrappers.removeValue(forKey: id)
  }

  func getWrapper(for id: String) -> WiFiAwareConnectionWrapper? {
    return wrappers[id]
  }

  deinit {
    for info in connectionsInfo.values {
      info.receiverTask.cancel()
      info.stateUpdateTask.cancel()
    }
    connections.removeAll()
    wrappers.removeAll()
  }
}

// MARK: - NetworkManager Actor (Matches Apple Sample Architecture)

@available(iOS 26.0, *)
actor NetworkManager: Sendable {
  private let logger = Logger(component: "NetworkManager")
  private var activeListener: NetworkListener<TCP>?
  private var activeBrowser: NWBrowser?
  private let browserQueue = DispatchQueue(label: "com.google.nearby.WiFiAware.NWBrowser")
  private var listenTask: Task<Void, Error>?
  private var browseTask: Task<Void, Error>?

  func waitForPairedDevices() async throws {
    for try await updatedDeviceList in WAPairedDevice.allDevices {
      if !updatedDeviceList.isEmpty {
        logger.info("Paired devices verified: \(updatedDeviceList.count)")
        break
      }
      logger.info("No paired devices yet, waiting...")
    }
  }

  /// - Parameter serviceName: the Wi-Fi Aware service to publish. Must be declared under
  ///   `WiFiAwareServices` in the app's Info.plist.
  func listen(
    serviceName: String, connectionManager: ConnectionManager,
    listenerWrapper: InternalListenerWrapper
  )
    async throws
  {
    logger.info("Start NetworkListener (service: \(serviceName))")
    cancelListen()

    try await waitForPairedDevices()

    try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
      // Holds the continuation until it is resumed; taking it out (and nil-ing the box) under the
      // lock guarantees it is resumed exactly once across the state handler and the catch path.
      let continuationBox = OSAllocatedUnfairLock<CheckedContinuation<Void, Error>?>(
        initialState: continuation)

      let task: Task<Void, Error> = Task(priority: .userInitiated) {
        do {
          let service = try WAPublishableService.named(serviceName)
          let listener = try NetworkListener(
            for: .wifiAware(.connecting(to: service, from: .allPairedDevices)),
            using: .parameters {
              TCP().noDelay(true).keepalive(idleTimeInSeconds: 5, count: 5, intervalInSeconds: 1)
            }
            .wifiAware { $0.performanceMode = appPerformanceMode }
            .serviceClass(appServiceClass)
          )
          self.activeListener = listener

          try await listener
            .onStateUpdate { listener, state in
              self.logger.info("Listener onStateUpdate: \(state)")
              switch state {
              case .setup, .waiting:
                break
              case .ready:
                self.logger.info("Listener ready!")
                if let c = continuationBox.withLock({
                  let c = $0
                  $0 = nil
                  return c
                }) {
                  c.resume(returning: ())
                }
              case .failed(let error):
                self.logger.error("Listener failed with error: \(error)")
                if let c = continuationBox.withLock({
                  let c = $0
                  $0 = nil
                  return c
                }) {
                  c.resume(throwing: error)
                }
              case .cancelled:
                self.logger.info("Listener cancelled")
                if let c = continuationBox.withLock({
                  let c = $0
                  $0 = nil
                  return c
                }) {
                  c.resume(
                    throwing: NSError(
                      domain: "NetworkManager", code: -1,
                      userInfo: [NSLocalizedDescriptionKey: "Listener cancelled"]))
                }
              @unknown default:
                break
              }
            }
            .run { connection in
              self.logger.info("Listener received connection: \(connection.id)")
              Task(priority: .userInitiated) {
                let connId = connection.id
                let wrapper = WiFiAwareConnectionWrapper(connection: connection) {
                  Task {
                    await connectionManager.stop(connId)
                  }
                }
                await connectionManager.add(connection, wrapper: wrapper)
                await listenerWrapper.addIncomingConnection(wrapper)
              }
            }
        } catch {
          if let c = continuationBox.withLock({
            let c = $0
            $0 = nil
            return c
          }) {
            c.resume(throwing: error)
          }
        }
      }

      listenTask = task
    }
  }

  /// - Parameter serviceName: the Wi-Fi Aware service to subscribe to. Must be declared under
  ///   `WiFiAwareServices` in the app's Info.plist.
  /// - Parameter peerDevice: the device we expect to find, when the peer identified itself and we
  ///   have a pairing on record for it. Restricting the browse to that one device is both more
  ///   precise and, with several devices paired, the only way to be sure we connect to the right
  ///   one — `wifip2pd` reports every allowed device and we would otherwise take whichever
  ///   answered first. Nil falls back to the whole paired store, which is what we have to do for
  ///   peers that send no identifier.
  func browse(
    serviceName: String, connectionManager: ConnectionManager, port: Int = 0,
    peerDevice: WAPairedDevice? = nil
  ) async throws
    -> WiFiAwareConnectionWrapper
  {
    logger.info("Start NetworkBrowser (service: \(serviceName), targetPort: \(port))")
    cancelBrowse()
    let service = try WASubscribableService.named(serviceName)

    try await waitForPairedDevices()
    // Brief pause to allow Android Synaptics firmware to finalize NAN pairing context (b/451755510)
    try? await Task.sleep(nanoseconds: 250_000_000)

    let devices: WASubscriberBrowser.Devices
    if let peerDevice {
      logger.info("browsing for paired device \(peerDevice.id) (\(peerDevice.displayName)) only")
      devices = .selected([peerDevice])
    } else {
      logger.info("browsing for all paired devices")
      devices = .allPairedDevices
    }
    let subscriber: WASubscriberBrowser = .wifiAware(
      .connecting(to: devices, from: service)
    )
    let descriptor = subscriber.makeDescriptor()
    let parameters = subscriber.configureParameters(nil)
    let browser = NWBrowser(for: descriptor, using: parameters)
    self.activeBrowser = browser

    do {
      // Discover the first endpoint WITHOUT cancelling NWBrowser so Subscriber 1 and nan0 stay
      // active in wifip2pd while NWConnection resolves and establishes the NAN Data Path.
      let discovered: (waEndpoint: WAEndpoint, nwEndpoint: NWEndpoint) =
        try await withTaskCancellationHandler {
          try await withCheckedThrowingContinuation {
            (continuation: CheckedContinuation<(WAEndpoint, NWEndpoint), Error>) in
            let resumedLock = OSAllocatedUnfairLock(initialState: false)

            browser.stateUpdateHandler = { [weak self] state in
              self?.logger.info("Browser onStateUpdate: \(state)")
              switch state {
              case .failed(let error):
                let alreadyResumed = resumedLock.withLock {
                  let old = $0
                  $0 = true
                  return old
                }
                if !alreadyResumed {
                  continuation.resume(throwing: error)
                }
              case .cancelled:
                let alreadyResumed = resumedLock.withLock {
                  let old = $0
                  $0 = true
                  return old
                }
                if !alreadyResumed {
                  continuation.resume(
                    throwing: NSError(
                      domain: "NetworkManager", code: -1,
                      userInfo: [
                        NSLocalizedDescriptionKey: "NWBrowser cancelled before discovery"
                      ]))
                }
              default:
                break
              }
            }

            browser.browseResultsChangedHandler = { [weak self] results, _ in
              self?.logger.info("Discovered browseResults: \(results.count)")
              for result in results {
                var resolvedWAEndpoint: WAEndpoint? = try? subscriber.makeEndpoint(from: result)
                // NWEndpoint.wifiAware was introduced in the iOS 26.4 SDK.
                // TODO: edwinwu - Remove the compiler check once Xcode 26.4 is the minimum version.
                #if compiler(>=6.3)
                  if resolvedWAEndpoint == nil, #available(iOS 26.4, *) {
                    resolvedWAEndpoint = result.endpoint.wifiAware
                  }
                #endif
                if let waEndpoint = resolvedWAEndpoint {
                  self?.logger.info(
                    " - Discovered WAEndpoint: \(waEndpoint), device: \(waEndpoint.device), nwEndpoint: \(result.endpoint)"
                  )
                  let alreadyResumed = resumedLock.withLock {
                    let old = $0
                    $0 = true
                    return old
                  }
                  if !alreadyResumed {
                    continuation.resume(returning: (waEndpoint, result.endpoint))
                  }
                  return
                }
              }
            }

            browser.start(queue: self.browserQueue)
          }
        } onCancel: {
          browser.cancel()
        }

      logger.info(
        "Browser discovered endpoint: \(discovered.waEndpoint), setting up connection (port: \(port)) while keeping NWBrowser active..."
      )
      let wrapper = try await connectionManager.setupConnection(
        to: discovered.waEndpoint,
        nwEndpoint: discovered.nwEndpoint,
        port: port
      )

      logger.info("Awaiting connection handshake (.ready) while NWBrowser remains active...")
      try await wrapper.awaitReady(timeoutSeconds: 30.0)
      logger.info("Connection ready and handshake completed! Stopping NWBrowser now.")
      cancelBrowse()
      return wrapper
    } catch {
      logger.error(
        "Discovery, connection setup, or handshake failed (\(error)), stopping NWBrowser.")
      cancelBrowse()
      throw error
    }
  }

  func cancelListen() {
    activeListener = nil
    listenTask?.cancel()
    listenTask = nil
  }

  func cancelBrowse() {
    if let browser = activeBrowser {
      logger.info("Cancelling active NetworkBrowser")
      browser.cancel()
      activeBrowser = nil
    }
    browseTask?.cancel()
    browseTask = nil
  }

  deinit {
    activeBrowser?.cancel()
    listenTask?.cancel()
    browseTask?.cancel()
  }
}

// MARK: - AwareManager (Objective-C Bridge)

@available(iOS 26.0, *)
@objc(GNCAwareManager)
public class AwareManager: NSObject, @unchecked Sendable {
  private let networkManager = NetworkManager()
  private let connectionManager = ConnectionManager()
  private let listenerLock = OSAllocatedUnfairLock(
    initialState: Optional<WiFiAwareConnectionWrapper>.none)
  private let latestConnectionLock = OSAllocatedUnfairLock(
    initialState: Optional<WiFiAwareConnectionWrapper>.none)
  private let browseTaskLock = OSAllocatedUnfairLock(initialState: Optional<Task<Void, Never>>.none)
  /// The in-flight `awaitNewPairedDevice` watch, if any. At most one is kept alive: a watch that
  /// outlives its pairing attempt would bind whatever device is paired next to the wrong peer.
  private let pairingWatchTaskLock = OSAllocatedUnfairLock(
    initialState: Optional<Task<Void, Never>>.none)
  private let logger = Logger(component: "AwareManager")

  @objc public var latestListenerWrapper: WiFiAwareConnectionWrapper? {
    get {
      return listenerLock.withLock { wrapper in
        if let existing = wrapper {
          return existing
        }
        let newWrapper = WiFiAwareConnectionWrapper(asListener: true) { [weak self] in
          self?.listenerLock.withLock { $0 = nil }
          self?.cancelListenTask()
        }
        wrapper = newWrapper
        return newWrapper
      }
    }
    set {
      listenerLock.withLock { $0 = newValue }
    }
  }

  @objc public override init() {
    super.init()
  }

  deinit {
    cancelListenTask()
    cancelBrowseTask()
    cancelPairingWatch()
  }

  /// Stops any `awaitNewPairedDevice` watch still waiting for a pairing.
  ///
  /// Call this once the pairing attempt it belongs to is over — the pairing UI was dismissed or
  /// subscribing stopped — so that a later pairing with a different peer is not bound to this
  /// attempt's `peerId`.
  @objc public func cancelPairingWatch() {
    let task = pairingWatchTaskLock.withLock { task in
      defer { task = nil }
      return task
    }
    guard let task else { return }
    logger.info("cancelPairingWatch called")
    task.cancel()
  }

  @objc public func cancelListenTask() {
    logger.info("cancelListenTask called")
    listenerLock.withLock { $0 = nil }
    let nm = networkManager
    Task {
      await nm.cancelListen()
    }
  }

  @objc public func cancelBrowseTask() {
    logger.info("cancelBrowseTask called")
    browseTaskLock.withLock { task in
      task?.cancel()
      task = nil
    }
    let nm = networkManager
    Task {
      await nm.cancelBrowse()
    }
  }

  @objc(listenWithServiceName:completionHandler:)
  public func listen(serviceName: String, completion: @escaping @Sendable (NSError?) -> Void) {
    logger.info("listen(serviceName: \(serviceName), completion:) called")
    guard let listenerWrapper = self.latestListenerWrapper,
      let internalListener = listenerWrapper.internalListenerWrapper
    else {
      logger.error("listen failed: latestListenerWrapper or internalListenerWrapper is nil")
      completion(
        NSError(
          domain: "AwareManager", code: -1,
          userInfo: [NSLocalizedDescriptionKey: "No listener wrapper available"]))
      return
    }

    Task(priority: .userInitiated) {
      do {
        try await self.networkManager.listen(
          serviceName: serviceName,
          connectionManager: self.connectionManager,
          listenerWrapper: internalListener
        )
        completion(nil)
      } catch {
        self.logger.error("listen error: \(error)")
        completion(error as NSError)
      }
    }
  }

  @objc(browseWithServiceName:port:peerId:completionHandler:)
  public func browse(
    serviceName: String, port: NSInteger, peerId: String,
    completion: @escaping @Sendable (NSError?) -> Void
  ) {
    logger.info(
      "browse(serviceName: \(serviceName), port: \(port), peerId: \(peerId), completion:) called")
    cancelBrowseTask()
    latestConnectionLock.withLock { $0 = nil }

    let task = Task(priority: .userInitiated) {
      let peerDevice = await PairedPeerRegistry.pairedDevice(forPeerId: peerId)
      do {
        let wrapper = try await self.networkManager.browse(
          serviceName: serviceName,
          connectionManager: self.connectionManager,
          port: port,
          peerDevice: peerDevice
        )
        self.latestConnectionLock.withLock { $0 = wrapper }
        completion(nil)
      } catch {
        let isTerminalDataPathFailure =
          (error as NSError).localizedDescription.contains(kTerminalDataPathFailureReason)
          || isTerminalWiFiAwareDataPathError(error)
        if isTerminalDataPathFailure {
          if !peerId.isEmpty {
            self.logger.error(
              "Evicting stale pairing for peer \(peerId) after terminal Wi-Fi Aware data-path error"
            )
            PairedPeerRegistry.forget(peerId: peerId)
          }
          self.logger.error("browse error: \(error)")
          completion(error as NSError)
          return
        }
        // A narrowed browse is the new behaviour and the riskier one: `.userSpecifiedDevices` with
        // an empty list is rejected outright by wifip2pd with -11992 ("has no Paired Devices"), and
        // if `.selected()` turns out to share that fate then every upgrade would fail. Retrying
        // unrestricted costs one round trip and keeps us no worse than before.
        if peerDevice != nil, !Task.isCancelled {
          self.logger.error(
            "narrowed browse failed (\(error)); retrying across all paired devices")
          do {
            let wrapper = try await self.networkManager.browse(
              serviceName: serviceName,
              connectionManager: self.connectionManager,
              port: port,
              peerDevice: nil
            )
            self.latestConnectionLock.withLock { $0 = wrapper }
            completion(nil)
            return
          } catch {
            self.logger.error("fallback browse also failed: \(error)")
            completion(error as NSError)
            return
          }
        }
        self.logger.error("browse error: \(error)")
        completion(error as NSError)
      }
    }
    browseTaskLock.withLock { $0 = task }
  }

  /// Reports whether we already hold a Wi-Fi Aware pairing with the peer identified by `peerId`.
  ///
  /// This is the question the subscribe gate actually needs answered. Asking instead whether *any*
  /// device is paired is what made a second Android device undiscoverable.
  ///
  /// An empty `peerId` means the peer sent no identifier — an Android build without the matching
  /// change, say — and always answers false, which keeps the old "show the pairing UI" behaviour
  /// for those peers.
  @objc(hasPairingForPeerId:completionHandler:)
  public func hasPairing(forPeerId peerId: String, completion: @escaping @Sendable (Bool) -> Void) {
    Task(priority: .userInitiated) {
      let device = await PairedPeerRegistry.pairedDevice(forPeerId: peerId)
      completion(device != nil)
    }
  }

  /// Waits for a pairing that did not exist when this was called, then binds it to `peerId`.
  ///
  /// Binding the *new* entry rather than "the" entry is what makes this work with more than one
  /// device paired: we snapshot the store first and take whichever ID appears afterwards.
  ///
  /// Only one watch is live at a time: calling this again replaces (and cancels) the previous
  /// watch, and `cancelPairingWatch()` ends it once its pairing attempt is over. Without that, a
  /// watch whose sheet was cancelled would sit on `allDevices` indefinitely and bind the next
  /// pairing — possibly with a different peer — to its stale `peerId`.
  ///
  /// As with `awaitPairedDevice`, `completion` is deliberately not called on failure or
  /// cancellation so that a caller racing this against the pairing UI's dismissal still has a
  /// release path.
  @objc(awaitNewPairedDeviceForPeerId:completionHandler:)
  public func awaitNewPairedDevice(
    forPeerId peerId: String, completion: @escaping @Sendable () -> Void
  ) {
    logger.info("awaitNewPairedDevice(forPeerId: \(peerId)) called")
    let task = Task(priority: .userInitiated) {
      let before = await PairedPeerRegistry.currentDeviceIds()
      do {
        for try await devices in WAPairedDevice.allDevices {
          // `allDevices` is not documented to end on cancellation, so check explicitly on every
          // update. This is also the last line of defence against binding a device that was
          // paired after this attempt was abandoned.
          try Task.checkCancellation()
          let added = Set(devices.keys).subtracting(before)
          guard let newId = added.first, let newDevice = devices[newId] else { continue }
          self.logger.info("new pairing \(newId) appeared, binding it to peer \(peerId)")
          PairedPeerRegistry.remember(peerId: peerId, device: newDevice)
          completion()
          return
        }
      } catch is CancellationError {
        self.logger.info("awaitNewPairedDevice(forPeerId: \(peerId)) cancelled")
      } catch {
        self.logger.error(
          "awaitNewPairedDevice failed, leaving the gate to the pairing UI: \(error)")
      }
    }
    let previous = pairingWatchTaskLock.withLock { slot in
      defer { slot = task }
      return slot
    }
    previous?.cancel()
  }

  /// Calls `completion` once the system reports at least one paired Wi-Fi Aware device.
  ///
  /// `WAPairedDevice.allDevices` publishes the new record the moment pairing succeeds, which is
  /// measurably earlier than the dismissal of Apple's pairing UI (~2.1 s in testing, spent on the
  /// picker's success animation). Callers that only need the pairing to *exist* — rather than the
  /// UI to be gone — can use this to skip that animation.
  ///
  /// `completion` is intentionally not called if the wait fails, so that a caller racing this
  /// against the UI dismissal still has a valid release path.
  @objc(awaitPairedDeviceWithCompletionHandler:)
  public func awaitPairedDevice(completion: @escaping @Sendable () -> Void) {
    logger.info("awaitPairedDevice(completion:) called")
    Task(priority: .userInitiated) {
      do {
        try await self.networkManager.waitForPairedDevices()
      } catch {
        self.logger.error("awaitPairedDevice failed, leaving the gate to the pairing UI: \(error)")
        return
      }
      completion()
    }
  }

  @objc public func getLatestConnectionWrapper() -> WiFiAwareConnectionWrapper? {
    return latestConnectionLock.withLock { $0 }
  }

  @objc public func getConnectionWrapper(for id: String) -> WiFiAwareConnectionWrapper? {
    return latestConnectionLock.withLock { $0 }
  }
}

// MARK: - UI & Pairing Presentation

@objc(GNCWiFiAwareMedium)
public class GNCWiFiAwareMedium: NSObject {
  private let logger = Logger(component: "SWIFT")

  @objc public override init() {
    super.init()
  }

  #if canImport(UIKit)
    @MainActor
    private func findTopViewController(controller: UIViewController? = nil) -> UIViewController? {
      let controller =
        controller
        ?? UIApplication.shared.connectedScenes
        .compactMap { $0 as? UIWindowScene }
        .flatMap { $0.windows }
        .first { $0.isKeyWindow }?.rootViewController
      if let navigationController = controller as? UINavigationController {
        return findTopViewController(controller: navigationController.visibleViewController)
      }
      if let tabBarController = controller as? UITabBarController {
        if let selected = tabBarController.selectedViewController {
          return findTopViewController(controller: selected)
        }
      }
      if let presented = controller?.presentedViewController {
        return findTopViewController(controller: presented)
      }
      return controller
    }

    @available(iOS 26.0, *)
    @MainActor
    @objc public func showAwarePublishingViewAtTop(
      serviceName: String, completion: @escaping () -> Void
    ) {
      self.logger.info("start of showAwarePublishingViewAtTop")
      let service: WAPublishableService
      do {
        service = try WAPublishableService.named(serviceName)
      } catch {
        self.logger.error("showAwarePublishingViewAtTop: \(error)")
        completion()
        return
      }
      let paringVC = DDDevicePairingViewController(
        listenerProvider: .wifiAware(.connecting(to: service, from: .userSpecifiedDevices)),
        access: .default
      )

      let tracker = DismissalTrackerViewController(onDismiss: completion)
      paringVC.addChild(tracker)
      paringVC.view.addSubview(tracker.view)
      tracker.view.frame = .zero
      tracker.view.isHidden = true
      tracker.didMove(toParent: paringVC)

      guard let topVC = findTopViewController() else {
        self.logger.error("findTopViewController returned nil in showAwarePublishingViewAtTop")
        completion()
        return
      }
      topVC.present(paringVC, animated: true, completion: nil)
    }

    @available(iOS 26.0, *)
    @MainActor
    @objc public func showAwareSubscribingViewAtTop(
      serviceName: String, completion: @escaping () -> Void
    ) {
      self.logger.info("start of showAwareSubscribingViewAtTop")
      let service: WASubscribableService
      do {
        service = try WASubscribableService.named(serviceName)
      } catch {
        self.logger.error("showAwareSubscribingViewAtTop: \(error)")
        completion()
        return
      }
      let subscriber: WASubscriberBrowser = .wifiAware(
        .connecting(to: .userSpecifiedDevices, from: service)
      )

      let descriptor = subscriber.makeDescriptor()
      let parameters = subscriber.configureParameters(NWParameters())

      guard
        let pickerVC = DDDevicePickerViewController(
          browseDescriptor: descriptor,
          parameters: parameters
        )
      else {
        self.logger.error("Failed to create DDDevicePickerViewController")
        completion()
        return
      }

      let tracker = DismissalTrackerViewController(onDismiss: completion)
      pickerVC.addChild(tracker)
      pickerVC.view.addSubview(tracker.view)
      tracker.view.frame = .zero
      tracker.view.isHidden = true
      tracker.didMove(toParent: pickerVC)

      guard let topVC = findTopViewController() else {
        self.logger.error("findTopViewController returned nil in showAwareSubscribingViewAtTop")
        completion()
        return
      }
      topVC.present(pickerVC, animated: true, completion: nil)
    }
  #else
    @objc public func showAwarePublishingViewAtTop(
      serviceName: String, completion: @escaping () -> Void
    ) {
      self.logger.info("showAwarePublishingViewAtTop not supported on this platform")
      completion()
    }

    @objc public func showAwareSubscribingViewAtTop(
      serviceName: String, completion: @escaping () -> Void
    ) {
      self.logger.info("showAwareSubscribingViewAtTop not supported on this platform")
      completion()
    }
  #endif

  @available(iOS 26.0, *)
  @objc public func getPairedDeviceCount() async -> NSInteger {
    return NSInteger(await logPairedDevices())
  }
}

@objc(GNCWiFiAwareSwiftWrapper)
public class GNCWiFiAwareSwiftWrapper: GNCWiFiAwareMedium {}

#if canImport(UIKit)
  @available(iOS 26.0, *)
  class DismissalTrackerViewController: UIViewController {
    let onDismiss: () -> Void
    private var hasDismissed = false

    init(onDismiss: @escaping () -> Void) {
      self.onDismiss = onDismiss
      super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
      fatalError("init(coder:) has not been implemented")
    }

    override func viewDidDisappear(_ animated: Bool) {
      super.viewDidDisappear(animated)
      guard !hasDismissed else { return }
      hasDismissed = true
      // Wait 1.5s settling delay (matches Apple Sample / SimulationEngine restart delay) to allow daemon & kernel to persist keys
      DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) { [self] in
        self.onDismiss()
      }
    }
  }
#endif
