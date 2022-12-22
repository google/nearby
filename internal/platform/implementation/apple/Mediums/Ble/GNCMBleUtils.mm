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

#import "internal/platform/implementation/apple/Mediums/Ble/GNCMBleUtils.h"

#include <sstream>
#include <string>

#import "internal/platform/implementation/apple/GNCUtils.h"
#import "internal/platform/implementation/apple/Mediums/Ble/Sockets/Source/Shared/GNSSocket.h"
#include "proto/mediums/ble_frames.pb.h"
#import "GoogleToolboxForMac/GTMLogger.h"

NS_ASSUME_NONNULL_BEGIN

static const uint8_t kGNCMControlPacketServiceIDHash[] = {0x00, 0x00, 0x00};
static const NSTimeInterval kBleSocketConnectionTimeout = 5.0;

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

NSData *GNCMParseBLEFramesIntroductionPacket(NSData *data) {
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
  GTMLoggerError(@"Unexpected -didReceiveData: call");
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
