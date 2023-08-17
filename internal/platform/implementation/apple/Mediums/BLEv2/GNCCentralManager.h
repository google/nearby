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

@protocol GNCCentralManagerDelegate;
@protocol GNCPeripheral;

NS_ASSUME_NONNULL_BEGIN

/** Protocol which helps create a fake of a @c CBCentralManager to inject for testing. */
@protocol GNCCentralManager

/** Shadow property of a @c CBCentralManagerDelegate. */
@property(weak, nonatomic, nullable) id<GNCCentralManagerDelegate> centralDelegate;

/**
 * The current state of the manager.
 *
 * This state is initially set to @c CBManagerStateUnknown. When the state updates, the manager
 * calls its delegate’s @c gnc_centralManagerDidUpdateState: method.
 */
@property(nonatomic, assign, readonly) CBManagerState state;

/**
 * Scans for peripherals that are advertising services.
 *
 * You can provide an array of @c CBUUID objects, representing service UUIDs, in the @c serviceUUIDs
 * parameter. When you do, the central manager returns only peripherals that advertise the services
 * you specify. If the @c serviceUUIDs parameter is @c nil, this method returns all discovered
 * peripherals, regardless of their supported services.
 *
 * @note The recommended practice is to populate the @c serviceUUIDs parameter rather than leaving
 * it @c nil.
 *
 * If the central manager is actively scanning with one set of parameters and it receives another
 * set to scan, the new parameters override the previous set. When the central manager discovers a
 * peripheral, it calls the @c gnc_centralManager:didDiscoverPeripheral:advertisementData:RSSI:
 * method of its delegate object.
 *
 * Your app can scan for Bluetooth devices in the background by specifying the @c bluetooth-central
 * background mode. To do this, your app must explicitly scan for one or more services by specifying
 * them in the @c serviceUUIDs parameter. The CBCentralManager scan option has no effect while
 * scanning in the background.
 *
 * @param serviceUUIDs An array of @c CBUUID objects that the app is interested in. Each @c CBUUID
 *                     object represents the UUID of a service that a peripheral advertises.
 * @param options A dictionary of options for customizing the scan.
 */
- (void)scanForPeripheralsWithServices:(nullable NSArray<CBUUID *> *)serviceUUIDs
                               options:(nullable NSDictionary<NSString *, id> *)options;

/**
 * Establishes a local connection to a peripheral.
 *
 * After successfully establishing a local connection to a peripheral, the central manager object
 * calls the @c gnc_centralManager:didConnectPeripheral: method of its delegate object. If the
 * connection attempt fails, the central manager object calls the
 * @c gnc_centralManager:didFailToConnectPeripheral:error: method of its delegate object instead.
 * Attempts to connect to a peripheral don’t time out. To explicitly cancel a pending connection to
 * a peripheral, call the @c cancelPeripheralConnection: method. Deallocating @c peripheral also
 * implicitly calls @c cancelPeripheralConnection:.
 *
 * @param peripheral The peripheral to which the central is attempting to connect.
 * @param options A dictionary to customize the behavior of the connection.
 */
- (void)connectPeripheral:(id<GNCPeripheral>)peripheral
                  options:(nullable NSDictionary<NSString *, id> *)options;

/** Asks the central manager to stop scanning for peripherals. */
- (void)stopScan;

@end

/**
 * Protocol which helps the @c GNCCentralManager wrap a @c CBCentralManagerDelegate for
 * testing.
 */
@protocol GNCCentralManagerDelegate <NSObject>

/**
 * Tells the delegate the central manager’s state updated.
 *
 * You implement this required method to ensure that the central device supports Bluetooth low
 * energy and that it’s available to use. You should issue commands to the central manager only when
 * the central manager’s @c state indicates it’s powered on. A state with a value lower than
 * @c CBManagerStatePoweredOn implies that scanning has stopped, which in turn disconnects any
 * previously-connected peripherals. If the state moves below @c CBManagerStatePoweredOff, all
 * @c CBPeripheral objects obtained from this central manager become invalid; you must retrieve or
 * discover these peripherals again.
 *
 * @param central The central manager whose state has changed.
 */
- (void)gnc_centralManagerDidUpdateState:(id<GNCCentralManager>)central;

/**
 * Tells the delegate the central manager discovered a peripheral while scanning for devices.
 *
 * You must retain a local copy of the peripheral if you want to perform commands on it. Use the
 * RSSI data to determine the proximity of a discoverable peripheral device, and whether you want to
 * connect to it automatically.
 *
 * @param central The central manager that provides the update.
 * @param peripheral The discovered peripheral.
 * @param advertisementData A dictionary containing any advertisement data.
 * @param RSSI The current received signal strength indicator (RSSI) of the peripheral, in decibels.
 */
- (void)gnc_centralManager:(id<GNCCentralManager>)central
     didDiscoverPeripheral:(id<GNCPeripheral>)peripheral
         advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                      RSSI:(NSNumber *)RSSI;

/**
 * Tells the delegate that the central manager connected to a peripheral.
 *
 * The manager invokes this method when a call to @c connectPeripheral:options: succeeds. You
 * typically implement this method to set the peripheral’s delegate and discover its services.
 *
 * @param central The central manager that provides this information.
 * @param peripheral The now-connected peripheral.
 */
- (void)gnc_centralManager:(id<GNCCentralManager>)central
      didConnectPeripheral:(id<GNCPeripheral>)peripheral;

/**
 * Tells the delegate the central manager failed to create a connection with a peripheral.
 *
 * The manager invokes this method when a connection initiated with the
 * @c connectPeripheral:options: method fails to complete. Because connection attempts don’t time
 * out, a failed connection usually indicates a transient issue, in which case you may attempt
 * connecting to the peripheral again.
 *
 * @param central The central manager that provides this information.
 * @param peripheral The peripheral that failed to connect.
 * @param error The cause of the failure, or @c nil if no error occurred.
 */
- (void)gnc_centralManager:(id<GNCCentralManager>)central
    didFailToConnectPeripheral:(id<GNCPeripheral>)peripheral
                         error:(nullable NSError *)error;

/**
 * Tells the delegate that the central manager disconnected from a peripheral.
 *
 * The manager invokes this method when disconnecting a peripheral previously connected with the
 * @c connectPeripheral:options: method. The error parameter contains the reason for the
 * disconnection, unless the disconnect resulted from a call to @c cancelPeripheralConnection:.
 *
 * All services, characteristics, and characteristic descriptors of a peripheral become invalidated
 * after it disconnects.
 *
 * @param central The central manager that provides this information.
 * @param peripheral The now-disconnected peripheral.
 * @param error The cause of the failure, or @c nil if no error occurred.
 */
- (void)gnc_centralManager:(id<GNCCentralManager>)central
    didDisconnectPeripheral:(id<GNCPeripheral>)peripheral
                      error:(nullable NSError *)error;

@end

/**
 * Declares that @c CBCentralManager implements the @c GNCCentralManager protocol.
 *
 * This allows us to directly use a @c CBCentralManager as a @c GNCCentralManager.
 */
@interface CBCentralManager () <GNCCentralManager>
@end

NS_ASSUME_NONNULL_END
