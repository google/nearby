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

@protocol GNCPeripheralDelegate;

NS_ASSUME_NONNULL_BEGIN

/** Protocol which helps create a fake of a @c CBPeripheral to inject for testing. */
@protocol GNCPeripheral

// This can't be @c delegate, because it would shadow @c CBPeripheral's delegate.
/**
 * The peripheral's delegate.
 * 
 * This delegate is only sent CBPeripheralDelegate messages, and the delegate is responsible for
 * forwarding those messages to its GNCPeripheralDelegate method implementations.
 * 
 * See: https://developer.apple.com/videos/play/wwdc2018/417/
 */
@property(weak, nonatomic, nullable) id<GNCPeripheralDelegate> peripheralDelegate;

/**
 * A list of a peripheral’s discovered services.
 *
 * Returns an array of services (represented by CBService objects) that successful a call to the
 * @c discoverServices: method discovered. If you haven’t yet called the @c discoverServices: method
 * to discover the services of the peripheral, or if there was an error in doing so, the value of
 * this property is @c nil.
 */
@property(retain, readonly, nullable) NSArray<CBService *> *services;

/**
 * The UUID associated with the peer.
 *
 * The value of this property represents the unique identifier of the peer. The first time a local
 * manager encounters a peer, the system assigns the peer a UUID, represented by a new @c NSUUID
 * object. Peers use @c NSUUID instances to identify themselves, instead of by the @c CBUUID objects
 * that identify a peripheral’s services, characteristics, and descriptors.
 */
@property(readonly, nonatomic) NSUUID *identifier;

/**
 * Discovers the specified services of the peripheral.
 *
 * You can provide an array of CBUUID objects, representing service UUIDs, in the @c serviceUUIDs
 * parameter. When you do, the peripheral returns only the services of the peripheral that match the
 * provided UUIDs.
 *
 * @note If the @c serviceUUIDs parameter is @c nil, this method returns all of the peripheral’s
 * available services. This is much slower than providing an array of service UUIDs to search for.
 *
 * When the peripheral discovers one or more services, it calls the
 * @c peripheral:didDiscoverServices: method of its delegate object. After a peripheral discovers
 * services, you can access them through the peripheral’s @c services property.
 *
 * @param serviceUUIDs An array of CBUUID objects that you are interested in. Each CBUUID object
 *                     represents a UUID that identifies the type of service you want to discover.
 */
- (void)discoverServices:(nullable NSArray<CBUUID *> *)serviceUUIDs;

/**
 * Discovers the specified characteristics of a service.
 *
 * You can provide an array of CBUUID objects, representing characteristic UUIDs, in the
 * @c characteristicUUIDs parameter. When you do, the peripheral returns only the characteristics of
 * the service that match the provided UUIDs. If the @c characteristicUUIDs parameter is @c nil,
 * this method returns all characteristics of the service.
 *
 * @note If the @c characteristicUUIDs parameter is @c nil, this method returns all of the service’s
 * characteristics. This is much slower than providing an array of characteristic UUIDs to search
 * for.
 *
 * When the peripheral discovers one or more characteristics of the specified service, it calls the
 * @c peripheral:didDiscoverCharacteristicsForService:error: method of its delegate object. After
 * the peripheral discovers the service’s characteristics, you can access them through the service’s
 * @c characteristics property.
 *
 * @param characteristicUUIDs An array of CBUUID objects that you are interested in. Each CBUUID
 *                            object represents a UUID that identifies the type of a characteristic
 *                            you want to discover.
 * @param service The service whose characteristics you want to discover.
 */
- (void)discoverCharacteristics:(nullable NSArray<CBUUID *> *)characteristicUUIDs
                     forService:(CBService *)service;

/**
 * Retrieves the value of a specified characteristic.
 *
 * When you call this method to read the value of a characteristic, the peripheral calls the
 * @c peripheral:didUpdateValueForCharacteristic:error: method of its delegate object. If the
 * peripheral successfully reads the value of the characteristic, you can access it through the
 * characteristic’s @c value property.
 *
 * Not all characteristics have a readable value. You can determine whether a characteristic’s value
 * is readable by accessing the relevant properties of the CBCharacteristicProperties enumeration.
 *
 * @param characteristic The characteristic whose value you want to read.
 */
- (void)readValueForCharacteristic:(CBCharacteristic *)characteristic;

@end

/**
 * Protocol which helps the @c GNCPeripheral wrap a @c CBPeripheralDelegate for
 * testing.
 */
@protocol GNCPeripheralDelegate <CBPeripheralDelegate>

/**
 * Tells the delegate that peripheral service discovery succeeded.
 *
 * Called when your app calls the @c discoverServices: method. If the peripheral successfully
 * discovers services, you can access them through the peripheral’s @c services property. If
 * successful, the @c error parameter is @c nil. If unsuccessful, the @c error parameter returns the
 * cause of the failure.
 *
 * @param peripheral The peripheral to which the services belong.
 * @param error The reason the call failed, or @c nil if no error occurred.
 */
- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral didDiscoverServices:(nullable NSError *)error;

/**
 * Tells the delegate that the peripheral found characteristics for a service.
 *
 * Called when your app calls the @c discoverCharacteristics:forService: method. If the peripheral
 * successfully discovers the characteristics of the specified service, you can access them through
 * the service’s @c characteristics property. If successful, the @c error parameter is @c nil. If
 * unsuccessful, the @c error parameter returns the cause of the failure.
 *
 * @param peripheral The peripheral providing this information.
 * @param service The service to which the characteristics belong.
 * @param error The reason the call failed, or @c nil if no error occurred.
 */
- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didDiscoverCharacteristicsForService:(CBService *)service
                                   error:(nullable NSError *)error;

/**
 * Tells the delegate that retrieving the specified characteristic’s value succeeded, or that the
 * characteristic’s value changed.
 *
 * Called when your app calls the @c readValueForCharacteristic: method. If successful, the @c error
 * parameter is @c nil. If unsuccessful, the @c error parameter returns the cause of the failure.
 *
 * @param peripheral The peripheral providing this information.
 * @param characteristic The characteristic containing the value.
 * @param error The reason the call failed, or @c nil if no error occurred.
 */
- (void)gnc_peripheral:(id<GNCPeripheral>)peripheral
    didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
                              error:(nullable NSError *)error;

@end

/**
 * Declares that @c CBPeripheral implements the @c GNCPeripheral protocol.
 *
 * This allows us to directly use a @c CBPeripheral as a @c GNCPeripheral.
 */
@interface CBPeripheral () <GNCPeripheral>
@end

NS_ASSUME_NONNULL_END
