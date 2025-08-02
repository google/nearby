// Copyright 2022 Google LLC
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

#import "internal/platform/implementation/apple/Mediums/BLE/GNCMBleUtils.h"

#include <sstream>
#include <string>
#include <vector>

#import "internal/platform/implementation/apple/GNCUtils.h"
#import "internal/platform/implementation/apple/Log/GNCLogger.h"
#import "internal/platform/implementation/apple/Mediums/BLE/Sockets/Source/Shared/GNSSocket.h"
#include "proto/mediums/ble_frames.pb.h"

NS_ASSUME_NONNULL_BEGIN

namespace {
constexpr uint8_t kGNCMControlPacketServiceIDHash[] = {0x00, 0x00, 0x00};
constexpr NSTimeInterval kBleSocketConnectionTimeout = 5.0;

bool IsSupportedCommand(GNCMBLEL2CAPCommand command) {
  return (command == GNCMBLEL2CAPCommandRequestAdvertisement ||
          command == GNCMBLEL2CAPCommandRequestAdvertisementFinish ||
          command == GNCMBLEL2CAPCommandRequestDataConnection ||
          command == GNCMBLEL2CAPCommandResponseAdvertisement ||
          command == GNCMBLEL2CAPCommandResponseServiceIdNotFound ||
          command == GNCMBLEL2CAPCommandResponseDataConnectionReady ||
          command == GNCMBLEL2CAPCommandResponseDataConnectionFailure);
}
}  // namespace

NSData *GNCMServiceIDHash(NSString *serviceID) {
  return [GNCSha256String(serviceID)
      subdataWithRange:NSMakeRange(0, GNCMBleAdvertisementLengthServiceIDHash)];
}

NSData *GNCMGenerateBLEFramesIntroductionPacket(NSData *serviceIDHash) {
  ::location::nearby::mediums::SocketControlFrame socket_control_frame;

  socket_control_frame.set_type(::location::nearby::mediums::SocketControlFrame::INTRODUCTION);
  auto *introduction_frame = socket_control_frame.mutable_introduction();
  introduction_frame->set_socket_version(::location::nearby::mediums::SocketVersion::V2);
  std::string service_id_hash((char *)serviceIDHash.bytes, (size_t)serviceIDHash.length);
  introduction_frame->set_service_id_hash(service_id_hash);

  NSMutableData *packet = [NSMutableData dataWithBytes:kGNCMControlPacketServiceIDHash
                                                length:sizeof(kGNCMControlPacketServiceIDHash)];
  std::ostringstream stream;
  socket_control_frame.SerializeToOstream(&stream);
  NSData *frameData = [NSData dataWithBytes:stream.str().data() length:stream.str().length()];
  [packet appendData:frameData];
  return packet;
}

NSData *_Nullable GNCMParseBLEFramesIntroductionPacket(NSData *data) {
  ::location::nearby::mediums::SocketControlFrame socket_control_frame;
  NSUInteger prefixLength = sizeof(kGNCMControlPacketServiceIDHash);
  NSData *packet = [data subdataWithRange:NSMakeRange(prefixLength, data.length - prefixLength)];
  if (socket_control_frame.ParseFromArray(packet.bytes, (int)packet.length)) {
    if (socket_control_frame.type() ==
            ::location::nearby::mediums::SocketControlFrame::INTRODUCTION &&
        socket_control_frame.has_introduction() &&
        socket_control_frame.introduction().has_socket_version() &&
        socket_control_frame.introduction().socket_version() ==
            ::location::nearby::mediums::SocketVersion::V2 &&
        socket_control_frame.introduction().has_service_id_hash()) {
      std::string service_id_hash = socket_control_frame.introduction().service_id_hash();
      return [NSData dataWithBytes:service_id_hash.data() length:service_id_hash.length()];
    }
  }

  return nil;
}

NSData *GNCMGenerateBLEFramesDisconnectionPacket(NSData *serviceIDHash) {
  ::location::nearby::mediums::SocketControlFrame socket_control_frame;

  socket_control_frame.set_type(::location::nearby::mediums::SocketControlFrame::DISCONNECTION);
  auto *disconnection_frame = socket_control_frame.mutable_disconnection();
  std::string service_id_hash((char *)serviceIDHash.bytes, (size_t)serviceIDHash.length);
  disconnection_frame->set_service_id_hash(service_id_hash);

  NSMutableData *packet = [NSMutableData dataWithBytes:kGNCMControlPacketServiceIDHash
                                                length:sizeof(kGNCMControlPacketServiceIDHash)];
  std::ostringstream stream;
  socket_control_frame.SerializeToOstream(&stream);
  NSData *frameData = [NSData dataWithBytes:stream.str().data() length:stream.str().length()];
  [packet appendData:frameData];
  return packet;
}

NSData *GNCMGenerateBLEFramesPacketAcknowledgementPacket(NSData *serviceIDHash, int receivedSize) {
  ::location::nearby::mediums::SocketControlFrame socket_control_frame;

  socket_control_frame.set_type(
      ::location::nearby::mediums::SocketControlFrame::PACKET_ACKNOWLEDGEMENT);
  auto *packet_acknowledgement_frame = socket_control_frame.mutable_packet_acknowledgement();
  std::string service_id_hash((char *)serviceIDHash.bytes, (size_t)serviceIDHash.length);
  packet_acknowledgement_frame->set_service_id_hash(service_id_hash);
  packet_acknowledgement_frame->set_received_size(receivedSize);

  NSMutableData *packet = [NSMutableData dataWithBytes:kGNCMControlPacketServiceIDHash
                                                length:sizeof(kGNCMControlPacketServiceIDHash)];
  std::ostringstream stream;
  socket_control_frame.SerializeToOstream(&stream);
  NSData *frameData = [NSData dataWithBytes:stream.str().data() length:stream.str().length()];
  [packet appendData:frameData];
  return packet;
}

@implementation GNCMBLEL2CAPPacket

- (instancetype)initWithCommand:(GNCMBLEL2CAPCommand)command data:(nullable NSData *)data {
  self = [super init];
  if (self) {
    _command = command;
    _data = data;
  }
  return self;
}

@end

// TODO: b/399815436 - Add unit tests for this function.
GNCMBLEL2CAPPacket *_Nullable GNCMParseBLEL2CAPPacket(NSData *data) {
  if (data.length < 1) {
    return nil;
  }

  // Extract command
  const uint8_t *bytes = static_cast<const uint8_t *>(data.bytes);
  NSUInteger receivedDataLength = [data length];
  GNCMBLEL2CAPCommand command = (GNCMBLEL2CAPCommand)bytes[0];
  if (!IsSupportedCommand(command)) {
    return nil;
  }

  // Extract data
  NSData *packetData = nil;
  if (receivedDataLength > 3) {
    // Extract data length (2 bytes, big endian)
    int dataLength = (bytes[1] << 8) | bytes[2];

    // Validate data length
    if (dataLength != (int)(receivedDataLength - 3)) {
      GNCLoggerError(@"[NEARBY] Data length mismatch. Expected: %d, Actual: %lu", dataLength,
                     receivedDataLength - 3);
      return nil;
    }
    if (dataLength > 0) {
      packetData = [NSData dataWithBytes:&bytes[3] length:dataLength];
    } else {
      packetData = nil;
    }
  }
  return [[GNCMBLEL2CAPPacket alloc] initWithCommand:command data:packetData];
}

// TODO: b/399815436 - Add unit tests for this function.
NSData *_Nullable GNCMGenerateBLEL2CAPPacket(GNCMBLEL2CAPCommand command, NSData *_Nullable data) {
  if (!IsSupportedCommand(command)) {
    GNCLoggerError(@"[NEARBY] Invalid command to generate packet: %lu", command);
    return nil;
  }

  std::vector<uint8_t> packet;
  packet.push_back((uint8_t)command);
  if (data != nil) {
    if (data.length > 65535) {
      GNCLoggerError(@"[NEARBY] Data length is too large: %lu", data.length);
      return nil;
    }
    // Prepare length bytes
    uint16_t value = (uint16_t)data.length;
    uint8_t bytes[2];
    // Store length in big-endian order.
    bytes[0] = (value >> 8) & 0xFF;
    bytes[1] = (value >> 0) & 0xFF;

    // Append length bytes to the packet
    packet.insert(packet.end(), bytes, bytes + 2);

    // Append the data to the packet
    const uint8_t *dataBytes = static_cast<const uint8_t *>(data.bytes);
    packet.insert(packet.end(), dataBytes, dataBytes + data.length);
  }

  return [NSData dataWithBytes:packet.data() length:packet.size()];
}

@interface GNCMBleSocketDelegate : NSObject <GNSSocketDelegate>
@property(nonatomic) GNCMBoolHandler connectedHandler;
@end

@implementation GNCMBleSocketDelegate

+ (instancetype)delegateWithConnectedHandler:(GNCMBoolHandler)connectedHandler {
  GNCMBleSocketDelegate *connection = [[GNCMBleSocketDelegate alloc] init];
  connection.connectedHandler = connectedHandler;
  return connection;
}

#pragma mark - GNSSocketDelegate

- (void)socketDidConnect:(GNSSocket *)socket {
  _connectedHandler(YES);
}

- (void)socket:(GNSSocket *)socket didDisconnectWithError:(NSError *)error {
  _connectedHandler(NO);
}

- (void)socket:(GNSSocket *)socket didReceiveData:(NSData *)data {
  GNCLoggerError(@"Unexpected -didReceiveData: call");
}

@end

void GNCMWaitForConnection(GNSSocket *socket, GNCMBoolHandler completion) {
  // This function passes YES to the completion when the socket has successfully connected, and
  // otherwise passes NO to the completion after a timeout of several seconds.  We shouldn't retain
  // the completion after it's been called, so store it in a __block variable and nil it out once
  // the socket has connected.
  __block GNCMBoolHandler completionRef = completion;

  // The delegate listens for the socket connection callbacks.  It's retained by the block passed to
  // dispatch_after below, so it will live long enough to do its job.
  GNCMBleSocketDelegate *delegate =
      [GNCMBleSocketDelegate delegateWithConnectedHandler:^(BOOL didConnect) {
        dispatch_async(dispatch_get_main_queue(), ^{
          if (completionRef) completionRef(didConnect);
          completionRef = nil;
        });
      }];
  socket.delegate = delegate;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, (int64_t)(kBleSocketConnectionTimeout * NSEC_PER_SEC)),
      dispatch_get_main_queue(), ^{
        (void)delegate;  // make sure it's retained until the timeout
        if (completionRef) completionRef(NO);
      });
}

NS_ASSUME_NONNULL_END
