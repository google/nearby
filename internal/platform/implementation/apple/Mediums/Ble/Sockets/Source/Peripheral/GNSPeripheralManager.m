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

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralManager+Private.h"

#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Peripheral/GNSPeripheralServiceManager+Private.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSSocket+Private.h"
#import "GoogleToolboxForMac/GTMLogger.h"

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#endif

static NSString *CBManagerStateString(CBManagerState state) {
  switch (state) {
    case CBManagerStateUnknown:
      return @"CBManagerStateUnknown";
    case CBManagerStateResetting:
      return @"CBManagerStateResetting";
    case CBManagerStateUnsupported:
      return @"CBManagerStateUnsupported";
    case CBManagerStateUnauthorized:
      return @"CBManagerStateUnauthorized";
    case CBManagerStatePoweredOff:
      return @"CBManagerStatePoweredOff";
    case CBManagerStatePoweredOn:
      return @"CBManagerStatePoweredOn";
  }
  return @"CBManagerState Unknown";
}

// http://b/28875581 On iOS, under some misterious conditions, the Bluetooth daemon (BTServer
// process) continously crashes when attempting to register Bluetooth services. In fact, if the
// BTServer process is killed, the OS spins a new instace of this process after 10 seconds.
//
// It is defined as a resetting episode a set of successive state changes of a CBPeripheralManager
// that include at least one state CBManagerStateResetting.
// As a heuristic, a resetting episode is considered a crash loop of the Bluetooth daemon iff:
// * the CBPeripheralManager enters more than 5 times in CBManagerStateResetting;
// * the time interval between successive CBManagerStateResetting notifications is
//   less than 15 seconds;
//
// The minimum number of successive CBManagerStateResetting states for a resetting
// episode to be considered a Bluetooth daemon crash loop.
static NSInteger gKBTCrashLoopMinNumberOfResetting = 5;

// The maximum time interval between successive CBManagerStateResetting states for a
// resetting episode to be considered a Bluetooth daemon crash loop.
static NSTimeInterval gKBTCrashLoopMaxTimeBetweenResetting = 15.f;

@interface GNSPeripheralManager () {
  // key: -[GNSPeripheralServiceManager serviceUUID], value: GNSPeripheralServiceManager
  NSMapTable *_peripheralServiceManagers;
  // Queue of blocks to update BLE values
  NSMutableDictionary<NSUUID *, NSMutableArray<GNSUpdateValueHandler> *>
      *_handlerQueuePerSocketIdentifier;
  NSDictionary<NSString *, id> *_advertisementInProgressData;
  NSDictionary<NSString *, id> *_advertisementData;
  dispatch_queue_t _queue;

  // Bluetooth daemon crash info.
  NSDate *_btCrashLastResettingDate;
  NSInteger _btCrashCount;
}

#if TARGET_OS_IPHONE
@property(nonatomic, assign) UIBackgroundTaskIdentifier backgroundTaskId;
#endif

@property(nonatomic, getter=isStarted) BOOL started;

@end

@implementation GNSPeripheralManager

- (instancetype)init {
  [self doesNotRecognizeSelector:_cmd];
  return nil;
}

- (instancetype)initWithAdvertisedName:(NSString *)advertisedName
                     restoreIdentifier:(NSString *)restoreIdentifier
                                 queue:(dispatch_queue_t)queue {
  self = [super init];
  if (self) {
    _restoreIdentifier = [restoreIdentifier copy];
    _advertisedName = [advertisedName copy];
    _handlerQueuePerSocketIdentifier = [NSMutableDictionary dictionary];
    _peripheralServiceManagers = [NSMapTable strongToWeakObjectsMapTable];
    _btCrashLastResettingDate = [NSDate dateWithTimeIntervalSince1970:0];
    _queue = queue;

#if TARGET_OS_IPHONE
    _backgroundTaskId = UIBackgroundTaskInvalid;
#endif
  }
  return self;
}

- (instancetype)initWithAdvertisedName:(NSString *)advertisedName
                     restoreIdentifier:(NSString *)restoreIdentifier {
  return [self initWithAdvertisedName:advertisedName
                    restoreIdentifier:restoreIdentifier
                                queue:dispatch_get_main_queue()];
}

- (void)dealloc {
  [self stop];
}

- (void)addPeripheralServiceManager:(GNSPeripheralServiceManager *)peripheralServiceManager
          bleServiceAddedCompletion:(GNSErrorHandler)completion {
  [_peripheralServiceManagers setObject:peripheralServiceManager
                                 forKey:peripheralServiceManager.serviceUUID];
  [peripheralServiceManager addedToPeripheralManager:self bleServiceAddedCompletion:completion];
  if (_started) {
    [self addBleServiceForServiceManager:peripheralServiceManager];
  }
  // Update all advertised services to make sure that the right services are advertised in case
  // all BLE services were already added.
  [self updateAdvertisedServices];
}

- (void)start {
  if (_started) {
    GTMLoggerInfo(@"Peripheral manager already started.");
    return;
  }
  NSMutableDictionary<NSString *, id> *options =
      [@{CBCentralManagerOptionShowPowerAlertKey : @NO} mutableCopy];
#if TARGET_OS_IPHONE
  // Restored API only supported on iOS.
  if (_restoreIdentifier) {
    options[CBPeripheralManagerOptionRestoreIdentifierKey] = _restoreIdentifier;
  }
#endif
  _cbPeripheralManager = [self cbPeripheralManagerWithDelegate:self queue:_queue options:options];
  GTMLoggerInfo(@"Peripheral manager started.");
  _started = YES;
  // From Apple documentation |-[GNSPeripheralManager peripheralManagerDidUpdateState:]| will be
  // called no matter the current bluetooth state. Also, if the CBPeripheralManager is being
  // restored by iOS, |-[GNSPeripheralManager peripheralManager:willRestoreState:]| will be called
  // before. Therefore, adding and advertising services have to be done in
  // |peripheralManagerDidUpdateState:| method, so that the restored services will not be added
  // (if there are restored services).
}

- (void)stop {
  if (!_started) {
    GTMLoggerInfo(@"Peripheral manager already stopped.");
    return;
  }
  GTMLoggerInfo(@"Peripheral manager stopped.");
  _started = NO;
  [self removeAllBleServicesAndStopAdvertising];
  _cbPeripheralManager.delegate = nil;
  _cbPeripheralManager = nil;
}

- (NSString *)description {
  return [NSString stringWithFormat:@"<%@ %p: started %@, services %@>", self.class, self,
                                    _started ? @"YES" : @"NO",
                                    _peripheralServiceManagers.keyEnumerator];
}

- (void)addBleServiceForServiceManager:(GNSPeripheralServiceManager *)peripheralServiceManager {
  if (peripheralServiceManager.cbServiceState != GNSBluetoothServiceStateNotAdded) {
    // Ignored if the peripheralServiceManager.cbService was already added.
    return;
  }
  [peripheralServiceManager willAddCBService];
  NSAssert(peripheralServiceManager.cbService, @"%@ had no cbService", peripheralServiceManager);
  [_cbPeripheralManager addService:peripheralServiceManager.cbService];
}

- (void)addAllBleServicesAndStartAdvertising {
  // Do not attempt to add any services if the Bluetooth Daemon is in a crash loop.
  if ([self isBTCrashLoop]) {
    // http://b/28875581 Adding new service when Bluetooth is powered off may be the reason for
    // the Bluetooth Daemon crash loop. Avoid adding any BLE services here and wait for the next
    // state change of the |_cbPeripheralManager|.
    GTMLoggerError(@"Bluetooth Daemon crash loop detected crashing. Avoid adding BLE services.");
    return;
  }

#if TARGET_OS_IPHONE
  if (_backgroundTaskId == UIBackgroundTaskInvalid) {
    GTMLoggerInfo(@"Start background task to add BLE services and start advertising.");
    _backgroundTaskId = [[UIApplication sharedApplication]
        beginBackgroundTaskWithName:@"Add BLE services and start advertising"
                  expirationHandler:^{
                    GTMLoggerError(@"Application was suspended before add BLE services and start "
                                   @"advertising finished.");
                    [[UIApplication sharedApplication] endBackgroundTask:_backgroundTaskId];
                    _backgroundTaskId = UIBackgroundTaskInvalid;
                  }];
  }
#endif

  for (GNSPeripheralServiceManager *peripheralServiceManager in _peripheralServiceManagers
           .objectEnumerator) {
    [self addBleServiceForServiceManager:peripheralServiceManager];
  }
  // Update all advertised services to make sure that the right services are advertised in case all
  // BLE services were already added.
  [self updateAdvertisedServices];
}

- (void)removeAllBleServicesAndStopAdvertising {
  [_cbPeripheralManager stopAdvertising];
  _advertisementInProgressData = nil;
  _advertisementData = nil;

  // Remove services and inform all service managers that their service was removed.
  [_cbPeripheralManager removeAllServices];
  for (GNSPeripheralServiceManager *peripheralServiceManager in _peripheralServiceManagers
           .objectEnumerator) {
    [peripheralServiceManager didRemoveCBService];
  }
}

- (void)updateAdvertisedServices {
  if (!_started) {
    // Do not start advertising if this peripheral manager is stopped.
    return;
  }

  CBManagerState cbState = _cbPeripheralManager.state;
  if ((cbState != CBManagerStatePoweredOn) && (cbState != CBManagerStatePoweredOff)) {
    GTMLoggerInfo(@"Do not start advertising on state %@", CBManagerStateString(cbState));
    return;
  }
  if (_advertisementInProgressData) {
    GTMLoggerInfo(@"Another start advertising operation is in progress.");
    return;
  }
  if (!_cbPeripheralManager.isAdvertising) {
    GTMLoggerInfo(@"Reset the advertiment data as the peripheral is not advertising.");
    _advertisementData = nil;
  }

  BOOL allServicesAdded = YES;
  NSMutableArray<CBUUID *> *serviceUUIDsToAdvertise = [NSMutableArray array];
  for (GNSPeripheralServiceManager *peripheralServiceManager in _peripheralServiceManagers
           .objectEnumerator) {
    if (peripheralServiceManager.cbServiceState != GNSBluetoothServiceStateAdded) {
      allServicesAdded = NO;
      break;
    }
    if (peripheralServiceManager.isAdvertising) {
      [serviceUUIDsToAdvertise addObject:peripheralServiceManager.serviceUUID];
    }
  }
  if (!allServicesAdded) {
    GTMLoggerInfo(@"Do not start advertising as not all bluetooth services are added.");
    return;
  }
  NSMutableDictionary<NSString *, id> *advertisementData = [NSMutableDictionary dictionary];
  advertisementData[CBAdvertisementDataServiceUUIDsKey] = serviceUUIDsToAdvertise;
  if (_advertisedName) {
    advertisementData[CBAdvertisementDataLocalNameKey] = _advertisedName;
  }
  if ([advertisementData isEqual:_advertisementData]) {
    GTMLoggerInfo(
        @"Finished adding BLE services and starting advertising (advertisement data = %@).",
        _advertisementData);
#if TARGET_OS_IPHONE
    [[UIApplication sharedApplication] endBackgroundTask:_backgroundTaskId];
    _backgroundTaskId = UIBackgroundTaskInvalid;
#endif
    return;
  }

  GTMLoggerInfo(@"Start advertising: %@", advertisementData);
  _advertisementInProgressData = [advertisementData copy];
  [_cbPeripheralManager stopAdvertising];
  [_cbPeripheralManager startAdvertising:_advertisementInProgressData];
}

#pragma mark - Private

- (void)updateOutgoingCharOnSocket:(GNSSocket *)socket withHandler:(GNSUpdateValueHandler)handler {
  NSUUID *socketIdentifier = socket.socketIdentifier;
  NSMutableArray<GNSUpdateValueHandler> *handlerQueue =
      _handlerQueuePerSocketIdentifier[socketIdentifier];
  if (!handlerQueue) {
    if (handler()) return;
    handlerQueue = [NSMutableArray array];
    _handlerQueuePerSocketIdentifier[socketIdentifier] = handlerQueue;
    GTMLoggerInfo(@"Queueing value for socket: %@", socketIdentifier);
  }
  [handlerQueue addObject:[handler copy]];
}

- (void)processUpdateValueBlocks {
  GTMLoggerInfo(@"About to send values for sockets: %@", _handlerQueuePerSocketIdentifier.allKeys);
  for (NSUUID *socketIdentifier in _handlerQueuePerSocketIdentifier.allKeys) {
    NSMutableArray<GNSUpdateValueHandler> *handlerQueue =
        _handlerQueuePerSocketIdentifier[socketIdentifier];
    GTMLoggerInfo(@"%lu queued values to send.", (unsigned long)handlerQueue.count);
    while (handlerQueue.count > 0) {
      GNSUpdateValueHandler handler = handlerQueue[0];
      if (!handler()) {
        GTMLoggerInfo(@"Value failed to be send, still in the queue to try later.");
        break;
      }
      GTMLoggerInfo(@"Value sent successfully.");
      [handlerQueue removeObjectAtIndex:0];
      if (handlerQueue.count == 0) {
        GTMLoggerInfo(@"Removing pending value queue for socket: %@", socketIdentifier);
        [_handlerQueuePerSocketIdentifier removeObjectForKey:socketIdentifier];
      }
    }
  }
}

- (BOOL)updateOutgoingCharacteristic:(NSData *)data onSocket:(GNSSocket *)socket {
  GNSPeripheralServiceManager *peripheralServiceManager = socket.owner;
  NSAssert(peripheralServiceManager, @"%@ should have an owner.", socket);
  return [_cbPeripheralManager updateValue:data
                         forCharacteristic:peripheralServiceManager.weaveOutgoingCharacteristic
                      onSubscribedCentrals:@[ socket.peerAsCentral ]];
}

- (CBPeripheralManager *)cbPeripheralManagerWithDelegate:(id<CBPeripheralManagerDelegate>)delegate
                                                   queue:(dispatch_queue_t)queue
                                                 options:(NSDictionary<NSString *, id> *)options {
  return [[CBPeripheralManager alloc] initWithDelegate:delegate queue:queue options:options];
}

- (void)socketDidDisconnect:(GNSSocket *)socket {
  NSAssert(!socket.connected, @"Should be disconnected");
  NSUUID *socketIdentifier = socket.socketIdentifier;
  NSMutableArray<GNSUpdateValueHandler> *handlerQueue =
      _handlerQueuePerSocketIdentifier[socketIdentifier];
  // The pending handlers have to be called so the associated send data completion blocks are
  // called with a disconnect error.
  for (GNSUpdateValueHandler handler in handlerQueue) {
    handler();
  }
  [_handlerQueuePerSocketIdentifier removeObjectForKey:socketIdentifier];
}

#pragma mark - Bluetooth Daemon Crash Loop

- (BOOL)isBTCrashLoop {
  NSTimeInterval timeSinceLastResetting = -[_btCrashLastResettingDate timeIntervalSinceNow];
  return (timeSinceLastResetting <= gKBTCrashLoopMaxTimeBetweenResetting) &&
         (_btCrashCount > gKBTCrashLoopMinNumberOfResetting);
}

- (void)updateBTCrashLoopHeuristic {
  NSAssert(_cbPeripheralManager.state == CBCentralManagerStateResetting, @"Unexpected CB state %@",
           CBManagerStateString(_cbPeripheralManager.state));
  NSDate *now = [NSDate date];
  if ([now timeIntervalSinceDate:_btCrashLastResettingDate] >
      gKBTCrashLoopMaxTimeBetweenResetting) {
    _btCrashCount = 0;
  }
  _btCrashLastResettingDate = now;
  _btCrashCount++;
}

#pragma mark - CBPeripheralManagerDelegate

- (void)peripheralManager:(CBPeripheralManager *)peripheral
         willRestoreState:(NSDictionary<NSString *, id> *)dict {
#if TARGET_OS_IPHONE
  // Restored API only supported on iOS.
  GTMLoggerInfo(@"Restore bluetooth services");
  _advertisementData = dict[CBPeripheralManagerRestoredStateAdvertisementDataKey];
  for (CBMutableService *service in dict[CBPeripheralManagerRestoredStateServicesKey]) {
    GNSPeripheralServiceManager *serviceManager =
        [_peripheralServiceManagers objectForKey:service.UUID];
    if (serviceManager) {
      [serviceManager restoredCBService:service];
    } else {
      GTMLoggerError(@"restoreFromCBService %@", service);
    }
  }
#endif
}

- (void)peripheralManagerDidUpdateState:(CBPeripheralManager *)peripheralManager {
  NSAssert(peripheralManager == _cbPeripheralManager, @"Wrong peripheral manager.");
  GTMLoggerInfo(@"Peripheral manager state updated: %@",
                CBManagerStateString(_cbPeripheralManager.state));
  switch (_cbPeripheralManager.state) {
    case CBManagerStatePoweredOn:
    case CBManagerStatePoweredOff:
      // As instructed by Apple engineers:
      // * BLE services should be added on power or on power off.
      // * BLE advertisment should be started on power on.
      //
      // Note 1: During testing it seems safe to start advertising when Bluetooth is turned off:
      // the advertisment resumes when the state changes to power on.
      //
      // Note 2: It is important to keep the added services and to keep advertising on power off,
      // to avoid the scenario when the application receives a power off notification right before
      // being suspended by the OS.
      [self addAllBleServicesAndStartAdvertising];
      break;
    case CBManagerStateResetting:
      // As instructed by Apple enginners, clean-up all internal state when CoreBluetooth is
      // resetting.
      [self updateBTCrashLoopHeuristic];
      [self removeAllBleServicesAndStopAdvertising];
      break;
    case CBManagerStateUnknown:
      // Clean-up all internal state when the CoreBluetooth state is unknown.
      [self removeAllBleServicesAndStopAdvertising];
      break;
    case CBManagerStateUnauthorized:
      // Clean-up all internal state if the application is not authorized to use Bluetooth.
      [self removeAllBleServicesAndStopAdvertising];
      break;
    case CBManagerStateUnsupported:
      // The application should have never attempted to start advertising if Bluetooth Low Energy
      // is not supported on the device.
      NSAssert(!peripheralManager.isAdvertising, @"Peripheral is not advertising");
      break;
  }
  if (_peripheralManagerStateHandler) {
    _peripheralManagerStateHandler(_cbPeripheralManager.state);
  }
}

- (void)peripheralManagerDidStartAdvertising:(CBPeripheralManager *)peripheral
                                       error:(NSError *)error {
  _advertisementData = _advertisementInProgressData;
  _advertisementInProgressData = nil;
  if (error) {
    GTMLoggerError(@"Start advertisment failed with error %@. Will retry to start advertising on "
                    "the next BLE state change notification. ",
                   error);
    _advertisementData = nil;
    return;
  }

  NSAssert(peripheral.isAdvertising, @"Peripheral should be advertising.");
  GTMLoggerInfo(@"Peripheral did start advertising %@", _advertisementData);

  // Once an advertisment operation is over, check if the advertised data is up-to-date.
  [self updateAdvertisedServices];
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
            didAddService:(CBService *)service
                    error:(NSError *)error {
  GNSPeripheralServiceManager *peripheralServiceManager =
      [_peripheralServiceManagers objectForKey:service.UUID];
  if (!peripheralServiceManager) {
    GTMLoggerError(@"Unknown service %@ with %@", service, self);
    return;
  }
  [peripheralServiceManager didAddCBServiceWithError:error];
  [self updateAdvertisedServices];
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
                         central:(CBCentral *)central
    didSubscribeToCharacteristic:(CBCharacteristic *)characteristic {
  GNSPeripheralServiceManager *peripheralServiceManager =
      [_peripheralServiceManagers objectForKey:characteristic.service.UUID];
  [peripheralServiceManager central:central didSubscribeToCharacteristic:characteristic];
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
                             central:(CBCentral *)central
    didUnsubscribeFromCharacteristic:(CBCharacteristic *)characteristic {
  GNSPeripheralServiceManager *peripheralServiceManager =
      [_peripheralServiceManagers objectForKey:characteristic.service.UUID];
  [peripheralServiceManager central:central didUnsubscribeFromCharacteristic:characteristic];

  // Restarting the peripheral manager after a disconnect. This is a workaround for b/31752176.
  // Update: This problem persists in iOS 10 & 11.  Discussion: https://goo.gl/fdE19G
  [self stop];
  [self start];
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didReceiveReadRequest:(CBATTRequest *)request {
  GNSPeripheralServiceManager *peripheralServiceManager =
      [_peripheralServiceManagers objectForKey:request.characteristic.service.UUID];
  if (!peripheralServiceManager) {
    [_cbPeripheralManager respondToRequest:request withResult:CBATTErrorAttributeNotFound];
    return;
  }
  CBATTError requestError = [peripheralServiceManager canProcessReadRequest:request];
  if (requestError != CBATTErrorSuccess) {
    [_cbPeripheralManager respondToRequest:request withResult:requestError];
    return;
  }
  [peripheralServiceManager processReadRequest:request];
  [_cbPeripheralManager respondToRequest:request withResult:CBATTErrorSuccess];
}

- (void)peripheralManager:(CBPeripheralManager *)peripheral
    didReceiveWriteRequests:(NSArray<CBATTRequest *> *)requests {
  // According to Apple's doc, if any one request cannot be processed, no request should
  // be processed. Only one CBATTError should be returned with the first request.
  if (requests.count == 0) {
    return;
  }
  CBATTError requestError = CBATTErrorSuccess;
  for (CBATTRequest *request in requests) {
    GNSPeripheralServiceManager *peripheralServiceManager =
        [_peripheralServiceManagers objectForKey:request.characteristic.service.UUID];
    if (!peripheralServiceManager) {
      requestError = CBATTErrorAttributeNotFound;
      break;
    }
    requestError = [peripheralServiceManager canProcessWriteRequest:request];
    if (requestError != CBATTErrorSuccess) {
      break;
    }
  }
  if (requestError != CBATTErrorSuccess) {
    [_cbPeripheralManager respondToRequest:requests[0] withResult:requestError];
    return;
  }
  for (CBATTRequest *request in requests) {
    GNSPeripheralServiceManager *peripheralServiceManager =
        [_peripheralServiceManagers objectForKey:request.characteristic.service.UUID];
    [peripheralServiceManager processWriteRequest:request];
  }
  [_cbPeripheralManager respondToRequest:requests[0] withResult:requestError];
}

- (void)peripheralManagerIsReadyToUpdateSubscribers:(CBPeripheralManager *)peripheral {
  GTMLoggerInfo(@"Ready to update subscribers.");
  [self processUpdateValueBlocks];
}

@end
