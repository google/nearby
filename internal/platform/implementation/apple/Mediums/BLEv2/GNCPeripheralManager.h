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

#import <CoreBluetooth/CoreBluetooth.h>
#import <Foundation/Foundation.h>

@protocol GNCPeripheralManagerDelegate;

NS_ASSUME_NONNULL_BEGIN

/** Protocol which helps create a fake of a @c CBPeripheralManager to inject for testing. */
@protocol GNCPeripheralManager

/** Shadow property of a @c CBPeripheralManagerDelegate. */
@property(weak, nonatomic, nullable) id<GNCPeripheralManagerDelegate> peripheralDelegate;

@property(nonatomic, assign, readonly) CBManagerState state;

@property(nonatomic, assign, readonly) BOOL isAdvertising;

/**
 * Publishes a service and any of its associated characteristics and characteristic descriptors to
 * the local GATT database.
 *
 * When you add a service to the database, the peripheral manager calls the
 * @c gnc_peripheralManager:didAddService:error: method of its delegate object. If the service
 * contains any included services, you must first publish them.
 *
 * @param service The service you want to publish.
 */
- (void)addService:(CBMutableService *)service;

/**
 * Removes a specified published service from the local GATT database.
 *
 * Because apps on the local peripheral device share the GATT database, more than one instance of a
 * service may exist in the database. As a result, this method removes only the instance of the
 * service that your app added to the database (using the @c addService: method). If any other
 * services contains this service, you must first remove them.
 *
 * @param service The service you want to remove.
 */
- (void)removeService:(CBMutableService *)service;

/**
 * Removes all published services from the local GATT database.
 *
 * Use this when you want to remove all services you’ve previously published, for example, if your
 * app has a toggle button to expose GATT services.
 *
 * Because apps on the local peripheral device share the GATT database, this method removes only the
 * services that you added using the @c addService: method. This call doesn’t remove any services
 * published by other apps on the local peripheral device.
 */
- (void)removeAllServices;

/**
 * Advertises peripheral manager data.
 *
 * When you start advertising peripheral data, the peripheral manager calls the
 * @c gnc_peripheralManagerDidStartAdvertising:error: method of its delegate object.
 *
 * Core Bluetooth advertises data on a “best effort” basis, due to limited space and because there
 * may be multiple apps advertising simultaneously. While in the foreground, your app can use up to
 * 28 bytes of space in the initial advertisement data for any combination of the supported
 * advertising data keys. If no this space remains, there’s an additional 10 bytes of space in the
 * scan response, usable only for the local name (represented by the value of the
 * @c CBAdvertisementDataLocalNameKey key). Note that these sizes don’t include the 2 bytes of
 * header information required for each new data type.
 *
 * Any service UUIDs contained in the value of the @c CBAdvertisementDataServiceUUIDsKey key that
 * don’t fit in the allotted space go to a special “overflow” area. These services are discoverable
 * only by an iOS device explicitly scanning for them.
 *
 * While your app is in the background, the local name isn’t advertised and all service UUIDs are in
 * the overflow area.
 *
 * For details about the format of advertising and response data, see the Bluetooth 4.0
 * specification, Volume 3, Part C, Section 11.
 *
 * @param advertisementData An optional dictionary containing the data you want to advertise. The
 *                          peripheral manager only supports two keys:
 *                          @c CBAdvertisementDataLocalNameKey and
 *                          @c CBAdvertisementDataServiceUUIDsKey.
 */
- (void)startAdvertising:(nullable NSDictionary<NSString *, id> *)advertisementData;

/**
 * Responds to a read or write request from a connected central.
 *
 * When the peripheral manager receives a request from a connected central to read or write a
 * characteristic’s value, it calls the @c gnc_peripheralManager:didReceiveReadRequest: or
 * @c gnc_peripheralManager:didReceiveWriteRequests: method of its delegate object. To respond to
 * the corresponding read or write request, you call this method whenever you recevie one of these
 * delegate method callbacks.
 *
 * @param request The read or write request received from the connected central. For more
 *                information about read and write requests, see @c CBATTRequest.
 * @param result The result of attempting to fulfill the request.
 */
- (void)respondToRequest:(CBATTRequest *)request withResult:(CBATTError)result;

/**
 * Stops advertising peripheral manager data.
 *
 * Call this method when you no longer want to advertise peripheral manager data.
 */
- (void)stopAdvertising;

@end

/**
 * Protocol which helps the @c GNCPeripheralManager wrap a @c CBPeripheralManagerDelegate for
 * testing.
 */
@protocol GNCPeripheralManagerDelegate <CBPeripheralManagerDelegate>

/**
 * Tells the delegate the peripheral manager’s state updated.
 *
 * You implement this required method to ensure that Bluetooth low energy is available to use on the
 * local peripheral device.
 *
 * Issue commands to the peripheral manager only when the peripheral manager is in the powered-on
 * state, as indicated by the @c CBPeripheralManagerStatePoweredOn constant. A state with a value
 * lower than @c CBPeripheralManagerStatePoweredOn implies that advertising has stopped and that any
 * connected centrals have been disconnected. If the state moves below
 * @c CBPeripheralManagerStatePoweredOff, advertising has stopped you must explicitly restart it. In
 * addition, the powered off state clears the local database; in this case you must explicitly
 * re-add all services. For a complete list and discussion of the possible values representing the
 * state of the peripheral manager, see the @c CBPeripheralManagerState enumeration in
 * @c CBPeripheralManager.
 *
 * @param peripheral The peripheral manager whose state has changed.
 */
- (void)gnc_peripheralManagerDidUpdateState:(id<GNCPeripheralManager>)peripheral;

/**
 * Tells the delegate the peripheral manager started advertising the local peripheral device’s data.
 *
 * Called when your app calls the @c startAdvertising: method to advertise the local peripheral
 * device’s data. If successful, the @c error parameter is @c nil. If a problem prevents advertising
 * the data, the @c error parameter returns the cause of the failure.
 *
 * @param peripheral The peripheral manager that is starting advertising.
 * @param error The reason the call failed, or @c nil if no error occurred.
 */
- (void)gnc_peripheralManagerDidStartAdvertising:(id<GNCPeripheralManager>)peripheral
                                           error:(nullable NSError *)error;

/**
 * Tells the delegate the peripheral manager published a service to the local GATT database.
 *
 * Called when your app calls the @c addService: method to publish a service to the local
 * peripheral’s GATT database. If the service published successfully to the local database, the
 * @c error parameter is @c nil. If unsuccessful, the @c error parameter provides the cause of the
 * failure.
 *
 * @param peripheral The peripheral manager adding the service.
 * @param service The service added to the local GATT database.
 * @param error The reason the call failed, or @c nil if no error occurred.
 */
- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
                didAddService:(CBService *)service
                        error:(nullable NSError *)error;

/**
 * Tells the delegate that a local peripheral received an Attribute Protocol (ATT) read request for
 * a characteristic with a dynamic value.
 *
 * When you receive this callback, call the @c respondToRequest:withResult: method of the
 * @c GNCPeripheralManager class exactly once to respond to the read request.
 *
 * @param peripheral The peripheral manager that received the request.
 * @param request A @c CBATTRequest object that represents a request to read a characteristic’s
 *                value.
 */
- (void)gnc_peripheralManager:(id<GNCPeripheralManager>)peripheral
        didReceiveReadRequest:(CBATTRequest *)request;

@end

@interface CBPeripheralManager () <GNCPeripheralManager>
@end

NS_ASSUME_NONNULL_END
