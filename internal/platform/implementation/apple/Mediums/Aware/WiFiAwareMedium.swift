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

  func error(_ message: String) {
    osLogger.error("\(message)")
  }
}
typealias Logger = LocalLogger

@available(iOS 26.0, *)
extension WAPublishableService {
  public static var qsService: WAPublishableService {
    guard let service = allServices["_qs-aware._tcp"] else {
      fatalError("Missing Wi-Fi Aware publishable service for _qs-aware._tcp")
    }
    return service
  }
}

@available(iOS 26.0, *)
extension WASubscribableService {
  public static var qsService: WASubscribableService {
    guard let service = allServices["_qs-aware._tcp"] else {
      fatalError("Missing Wi-Fi Aware subscribable service for _qs-aware._tcp")
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

  func signalClosed() {
    guard !isClosed else { return }
    isClosed = true
    continuation?.resume()
    continuation = nil
    readyContinuation?.resume(
      throwing: NSError(
        domain: "InternalConnectionWrapper", code: -1,
        userInfo: [NSLocalizedDescriptionKey: "Connection closed during handshake"]))
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

  func signalClosed() {
    Task {
      await internalWrapper?.signalClosed()
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

  func setupConnection(to endpoint: WAEndpoint, port: Int = 0) async throws
    -> WiFiAwareConnectionWrapper
  {
    let connection = NetworkConnection(
      to: endpoint,
      using: .parameters {
        TCP().noDelay(true).keepalive(idleTimeInSeconds: 5, count: 5, intervalInSeconds: 1)
      }
      .wifiAware { $0.performanceMode = appPerformanceMode }
      .serviceClass(appServiceClass)
    )

    let id = connection.id
    logger.info("Set up connection: \(id) to endpoint: \(endpoint)")

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

  func listen(connectionManager: ConnectionManager, listenerWrapper: InternalListenerWrapper)
    async throws
  {
    logger.info("Start NetworkListener")
    cancelListen()

    try await waitForPairedDevices()

    try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, Error>) in
      let resumedLock = OSAllocatedUnfairLock(initialState: false)

      let task: Task<Void, Error> = Task(priority: .userInitiated) {
        do {
          let listener = try NetworkListener(
            for: .wifiAware(.connecting(to: .qsService, from: .allPairedDevices)),
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
                let alreadyResumed = resumedLock.withLock {
                  let old = $0
                  $0 = true
                  return old
                }
                if !alreadyResumed { continuation.resume(returning: ()) }
              case .failed(let error):
                self.logger.error("Listener failed with error: \(error)")
                let alreadyResumed = resumedLock.withLock {
                  let old = $0
                  $0 = true
                  return old
                }
                if !alreadyResumed { continuation.resume(throwing: error) }
              case .cancelled:
                self.logger.info("Listener cancelled")
                let alreadyResumed = resumedLock.withLock {
                  let old = $0
                  $0 = true
                  return old
                }
                if !alreadyResumed {
                  continuation.resume(
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
          let alreadyResumed = resumedLock.withLock {
            let old = $0
            $0 = true
            return old
          }
          if !alreadyResumed { continuation.resume(throwing: error) }
        }
      }

      listenTask = task
    }
  }

  func browse(connectionManager: ConnectionManager, port: Int = 0) async throws
    -> WiFiAwareConnectionWrapper
  {
    logger.info("Start NetworkBrowser (targetPort: \(port))")
    cancelBrowse()

    try await waitForPairedDevices()
    // Brief pause to allow Android Synaptics firmware to finalize NAN pairing context (b/451755510)
    try? await Task.sleep(nanoseconds: 250_000_000)

    let browser = NetworkBrowser(
      for: .wifiAware(.connecting(to: .allPairedDevices, from: .qsService))
    )
    .onStateUpdate { browser, state in
      self.logger.info("Browser onStateUpdate: \(state)")
    }

    // Connect to the first discovered endpoint (clean discovery pattern from BuildingPeerToPeerApps)
    let endpoint = try await browser.run { waEndpoints in
      self.logger.info("Discovered endpoints: \(waEndpoints.count)")
      for ep in waEndpoints {
        self.logger.info(" - Discovered endpoint: \(ep), device: \(ep.device)")
      }
      if let firstEndpoint = waEndpoints.first {
        return .finish(firstEndpoint)
      } else {
        return .continue
      }
    }

    logger.info(
      "Browser discovered endpoint: \(endpoint), setting up connection (port: \(port))...")
    let wrapper = try await connectionManager.setupConnection(to: endpoint, port: port)

    logger.info("Awaiting connection handshake (.ready)...")
    try await wrapper.awaitReady(timeoutSeconds: 30.0)
    logger.info("Connection ready and handshake completed!")
    return wrapper
  }

  func cancelListen() {
    activeListener = nil
    listenTask?.cancel()
    listenTask = nil
  }

  func cancelBrowse() {
    browseTask?.cancel()
    browseTask = nil
  }

  deinit {
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

  @objc(listenWithCompletionHandler:)
  public func listen(completion: @escaping @Sendable (NSError?) -> Void) {
    logger.info("listen(completion:) called")
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

  @objc(browseWithPort:completionHandler:)
  public func browse(port: NSInteger, completion: @escaping @Sendable (NSError?) -> Void) {
    logger.info("browse(port: \(port), completion:) called")
    cancelBrowseTask()
    latestConnectionLock.withLock { $0 = nil }

    let task = Task(priority: .userInitiated) {
      do {
        let wrapper = try await self.networkManager.browse(
          connectionManager: self.connectionManager,
          port: port
        )
        self.latestConnectionLock.withLock { $0 = wrapper }
        completion(nil)
      } catch {
        self.logger.error("browse error: \(error)")
        completion(error as NSError)
      }
    }
    browseTaskLock.withLock { $0 = task }
  }

  @objc(browseWithCompletionHandler:)
  public func browse(completion: @escaping @Sendable (NSError?) -> Void) {
    browse(port: 0, completion: completion)
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
    @objc public func showAwarePublishingViewAtTop(completion: @escaping () -> Void) {
      self.logger.info("start of showAwarePublishingViewAtTop")
      let paringVC = DDDevicePairingViewController(
        listenerProvider: .wifiAware(.connecting(to: .qsService, from: .userSpecifiedDevices)),
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
    @objc public func showAwareSubscribingViewAtTop(completion: @escaping () -> Void) {
      self.logger.info("start of showAwareSubscribingViewAtTop")
      let subscriber: WASubscriberBrowser = .wifiAware(
        .connecting(to: .userSpecifiedDevices, from: .qsService)
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
    @objc public func showAwarePublishingViewAtTop(completion: @escaping () -> Void) {
      self.logger.info("showAwarePublishingViewAtTop not supported on this platform")
      completion()
    }

    @objc public func showAwareSubscribingViewAtTop(completion: @escaping () -> Void) {
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
