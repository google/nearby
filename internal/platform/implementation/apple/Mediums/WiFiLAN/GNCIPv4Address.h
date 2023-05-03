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

#import <Foundation/Foundation.h>

/** A container for IPv4 address information. */
@interface GNCIPv4Address : NSObject

/**
 * @remark init is not an available initializer.
 */
- (nonnull instancetype)init NS_UNAVAILABLE;

/**
 * Creates a container for IPv4 address information.
 *
 * @param byte1 The first byte of the IP address.
 * @param byte2 The second byte of the IP address.
 * @param byte3 The third byte of the IP address.
 * @param byte4 The forth byte of the IP address.
 */
- (nonnull instancetype)initWithByte1:(uint8_t)byte1
                                byte2:(uint8_t)byte2
                                byte3:(uint8_t)byte3
                                byte4:(uint8_t)byte4 NS_DESIGNATED_INITIALIZER;

+ (nonnull instancetype)addressFromFourByteInt:(uint32_t)address;

+ (nonnull instancetype)addressFromData:(nonnull NSData *)address;

/** The first byte of the IP address. */
@property(nonatomic, readonly) uint8_t byte1;

/** The second byte of the IP address. */
@property(nonatomic, readonly) uint8_t byte2;

/** The third byte of the IP address. */
@property(nonatomic, readonly) uint8_t byte3;

/** The forth byte of the IP address. */
@property(nonatomic, readonly) uint8_t byte4;

/** The 4 byte binary representation for the IPv4 address. */
@property(nonatomic, nonnull, readonly) NSData *binaryRepresentation;

/** The human readable dotted representation for the IPv4 address. */
@property(nonatomic, nonnull, readonly) NSString *dottedRepresentation;

@end
