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

#import "internal/platform/implementation/ios/wifi_lan.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/container/internal/common.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "internal/platform/cancellation_flag.h"
#include "internal/platform/exception.h"
#import "internal/platform/implementation/ios/Mediums/GNCMConnection.h"
#import "internal/platform/implementation/ios/Mediums/WifiLan/GNCMBonjourBrowser.h"
#import "internal/platform/implementation/ios/Mediums/WifiLan/GNCMBonjourService.h"
#include "internal/platform/implementation/ios/utils.h"
#include "internal/platform/implementation/wifi_lan.h"
#include "internal/platform/nsd_service_info.h"
#include "internal/platform/prng.h"
#import "GoogleToolboxForMac/GTMLogger.h"

namespace location {
namespace nearby {
namespace ios {

/** WifiLanInputStream implementation. */
WifiLanInputStream::WifiLanInputStream()
    : newDataPackets_([NSMutableArray array]),
      accumulatedData_([NSMutableData data]),
      condition_([[NSCondition alloc] init]) {
  // Create the handlers of incoming data from the remote endpoint.
  connectionHandlers_ = [GNCMConnectionHandlers
      payloadHandler:^(NSData* data) {
        [condition_ lock];
        // Add the incoming data to the data packet array to be processed in Read() below.
        [newDataPackets_ addObject:data];
        [condition_ signal];
        [condition_ unlock];
      }
      disconnectedHandler:^{
        [condition_ lock];
        // Release the data packet array, meaning the stream has been closed or severed.
        newDataPackets_ = nil;
        [condition_ signal];
        [condition_ unlock];
      }];
}

WifiLanInputStream::~WifiLanInputStream() {
  NSCAssert(!newDataPackets_, @"WifiLanInputStream not closed before destruction");
}

ExceptionOr<ByteArray> WifiLanInputStream::Read(std::int64_t size) {
  // Block until either (a) the connection has been closed, or (b) enough data to return.
  NSData* dataToReturn;
  [condition_ lock];
  while (true) {
    // Check if the stream has been closed or severed.
    if (!newDataPackets_) break;

    if (newDataPackets_.count > 0) {
      // Add the packet data to the accumulated data.
      for (NSData* data in newDataPackets_) {
        if (data.length > 0) {
          [accumulatedData_ appendData:data];
        }
      }
      [newDataPackets_ removeAllObjects];
    }

    if ((size == -1) && (accumulatedData_.length > 0)) {
      // Return all of the data.
      dataToReturn = accumulatedData_;
      accumulatedData_ = [NSMutableData data];
      break;
    } else if (accumulatedData_.length > 0) {
      // Return up to |size| bytes of the data.
      std::int64_t sizeToReturn = accumulatedData_.length < size ? accumulatedData_.length : size;
      NSRange range = NSMakeRange(0, (NSUInteger)sizeToReturn);
      dataToReturn = [accumulatedData_ subdataWithRange:range];
      [accumulatedData_ replaceBytesInRange:range withBytes:nil length:0];
      break;
    }

    [condition_ wait];
  }
  [condition_ unlock];

  if (dataToReturn) {
    GTMLoggerInfo(@"[NEARBY] Input stream: Received data of size: %lu",
                  (unsigned long)dataToReturn.length);
    return ExceptionOr<ByteArray>(ByteArrayFromNSData(dataToReturn));
  } else {
    return ExceptionOr<ByteArray>{Exception::kIo};
  }
}

Exception WifiLanInputStream::Close() {
  // Unblock pending read operation.
  [condition_ lock];
  newDataPackets_ = nil;
  [condition_ signal];
  [condition_ unlock];
  return {Exception::kSuccess};
}

/** WifiLanOutputStream implementation. */
WifiLanOutputStream::~WifiLanOutputStream() {
  NSCAssert(!connection_, @"WifiLanOutputStream not closed before destruction");
}

Exception WifiLanOutputStream::Write(const ByteArray& data) {
  [condition_ lock];
  GTMLoggerDebug(@"[NEARBY] Sending data of size: %lu",
                 (unsigned long)NSDataFromByteArray(data).length);

  NSMutableData* packet = [NSMutableData dataWithData:NSDataFromByteArray(data)];

  // Send the data, blocking until the completion handler is called.
  __block GNCMPayloadResult sendResult = GNCMPayloadResultFailure;
  __block bool isComplete = NO;
  NSCondition* condition = condition_;  // don't capture |this| in completion
  // Check if connection_ is nil, then just don't wait and return as failure.
  if (connection_ != nil) {
    [connection_ sendData:packet
        progressHandler:^(size_t count) {
        }
        completion:^(GNCMPayloadResult result) {
          // Make sure we haven't already reported completion before. This prevents a crash
          // where we try leaving a dispatch group more times than we entered it.
          // b/79095653.
          if (isComplete) {
            return;
          }
          isComplete = YES;
          sendResult = result;
          [condition lock];
          [condition signal];
          [condition unlock];
        }];
    [condition_ wait];
    [condition_ unlock];
  } else {
    sendResult = GNCMPayloadResultFailure;
    [condition_ unlock];
  }

  if (sendResult == GNCMPayloadResultSuccess) {
    return {Exception::kSuccess};
  } else {
    return {Exception::kIo};
  }
}

Exception WifiLanOutputStream::Flush() {
  // The Write() function block until the data is received by the remote endpoint, so there's
  // nothing to do here.
  return {Exception::kSuccess};
}

Exception WifiLanOutputStream::Close() {
  // Unblock pending write operation.
  [condition_ lock];
  connection_ = nil;
  [condition_ signal];
  [condition_ unlock];
  return {Exception::kSuccess};
}

/** WifiLanSocket implementation. */
WifiLanSocket::WifiLanSocket(id<GNCMConnection> connection)
    : input_stream_(new WifiLanInputStream()),
      output_stream_(new WifiLanOutputStream(connection)) {}

WifiLanSocket::~WifiLanSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

bool WifiLanSocket::IsClosed() const {
  absl::MutexLock lock(&mutex_);
  return closed_;
}

Exception WifiLanSocket::Close() {
  absl::MutexLock lock(&mutex_);
  DoClose();
  return {Exception::kSuccess};
}

void WifiLanSocket::DoClose() {
  if (!closed_) {
    input_stream_->Close();
    output_stream_->Close();
    closed_ = true;
  }
}

/** WifiLanServerSocket implementation. */
std::string WifiLanServerSocket::GetName(const std::string& ip_address, int port) {
  std::string dot_delimited_string;
  if (!ip_address.empty()) {
    for (auto byte : ip_address) {
      if (!dot_delimited_string.empty()) absl::StrAppend(&dot_delimited_string, ".");
      absl::StrAppend(&dot_delimited_string, absl::StrFormat("%d", byte));
    }
  }
  std::string out = absl::StrCat(dot_delimited_string, ":", port);
  return out;
}

WifiLanServerSocket::~WifiLanServerSocket() {
  absl::MutexLock lock(&mutex_);
  DoClose();
}

std::unique_ptr<api::WifiLanSocket> WifiLanServerSocket::Accept() {
  absl::MutexLock lock(&mutex_);
  while (!closed_ && pending_sockets_.empty()) {
    cond_.Wait(&mutex_);
  }
  // Return early if closed.
  if (closed_) return {};

  auto remote_socket = std::move(pending_sockets_.extract(pending_sockets_.begin()).value());
  return std::move(remote_socket);
}

bool WifiLanServerSocket::Connect(std::unique_ptr<WifiLanSocket> socket) {
  absl::MutexLock lock(&mutex_);
  if (closed_) {
    return false;
  }
  // add client socket to the pending list
  pending_sockets_.insert(std::move(socket));
  cond_.SignalAll();
  if (closed_) {
    return false;
  }
  return true;
}

void WifiLanServerSocket::SetCloseNotifier(std::function<void()> notifier) {
  absl::MutexLock lock(&mutex_);
  close_notifier_ = std::move(notifier);
}

Exception WifiLanServerSocket::Close() {
  absl::MutexLock lock(&mutex_);
  return DoClose();
}

Exception WifiLanServerSocket::DoClose() {
  bool should_notify = !closed_;
  closed_ = true;
  if (should_notify) {
    cond_.SignalAll();
    if (close_notifier_) {
      auto notifier = std::move(close_notifier_);
      mutex_.Unlock();
      // Notifier may contain calls to public API, and may cause deadlock, if
      // mutex_ is held during the call.
      notifier();
      mutex_.Lock();
    }
  }
  return {Exception::kSuccess};
}

/** WifiLanMedium implementation. */
WifiLanMedium::~WifiLanMedium() {
  advertising_info_.Clear();
  discovering_info_.Clear();
}

bool WifiLanMedium::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
  std::string service_type = nsd_service_info.GetServiceType();
  NSString* serviceType = ObjCStringFromCppString(service_type);
  {
    absl::MutexLock lock(&mutex_);
    if (advertising_info_.Existed(service_type)) {
      GTMLoggerInfo(@"[NEARBY] WifiLan StartAdvertising: Can't start advertising because "
                    @"service_type=%@, has started already",
                    serviceType);
      return false;
    }
  }

  // Retrieve service name.
  NSString* serviceName = ObjCStringFromCppString(nsd_service_info.GetServiceName());
  // Retrieve TXTRecord and convert it to NSDictionary type.
  NSDictionary<NSString*, NSData*>* TXTRecordData =
      NSDictionaryFromCppTxtRecords(nsd_service_info.GetTxtRecords());
  // Get ip address and port to retrieve the server_socket, if not, then return nil.
  std::string ip_address = nsd_service_info.GetIPAddress();
  int port = nsd_service_info.GetPort();
  __block std::string socket_name = WifiLanServerSocket::GetName(ip_address, port);
  GNCMBonjourService* service = [[GNCMBonjourService alloc]
           initWithServiceName:serviceName
                   serviceType:serviceType
                          port:requesting_port_
                 TXTRecordData:TXTRecordData
      endpointConnectedHandler:^GNCMConnectionHandlers*(id<GNCMConnection> connection) {
        auto item = server_sockets_.find(socket_name);
        WifiLanServerSocket* server_socket = item != server_sockets_.end() ? item->second : nullptr;
        if (!server_socket) {
          return nil;
        }
        auto socket = absl::make_unique<WifiLanSocket>(connection);
        GNCMConnectionHandlers* connectionHandlers =
            static_cast<WifiLanInputStream&>(socket->GetInputStream()).GetConnectionHandlers();
        server_socket->Connect(std::move(socket));
        return connectionHandlers;
      }];
  {
    absl::MutexLock lock(&mutex_);
    advertising_info_.Add(service_type, service);
  }
  return true;
}

bool WifiLanMedium::StopAdvertising(const NsdServiceInfo& nsd_service_info) {
  std::string service_type = nsd_service_info.GetServiceType();
  {
    absl::MutexLock lock(&mutex_);
    if (!advertising_info_.Existed(service_type)) {
      GTMLoggerInfo(@"[NEARBY] WifiLan StopAdvertising: Can't stop advertising because we never "
                    @"started advertising for service_type=%@",
                    ObjCStringFromCppString(service_type));
      return false;
    }
    advertising_info_.Remove(service_type);
  }
  return true;
}

bool WifiLanMedium::StartDiscovery(const std::string& service_type,
                                   DiscoveredServiceCallback callback) {
  NSString* serviceType = ObjCStringFromCppString(service_type);
  {
    absl::MutexLock lock(&mutex_);
    if (discovering_info_.Existed(service_type)) {
      GTMLoggerInfo(@"[NEARBY] WifiLan StartAdvertising: Can't start discovery because "
                    @"service_type=%@, has started already",
                    serviceType);
      return false;
    }
  }
  GNCMBonjourBrowser* browser = [[GNCMBonjourBrowser alloc]
       initWithServiceType:serviceType
      endpointFoundHandler:^GNCMEndpointLostHandler(
          NSString* endpointId, NSString* serviceType, NSString* serviceName,
          NSDictionary<NSString*, NSData*>* _Nullable txtRecordData,
          GNCMConnectionRequester requestConnection) {
        __block NsdServiceInfo nsd_service_info = {};
        nsd_service_info.SetServiceName(CppStringFromObjCString(serviceName));
        nsd_service_info.SetServiceType(CppStringFromObjCString(serviceType));
        // Set TXTRecord converted from NSDictionary to hash map.
        if (txtRecordData != nil) {
          auto txt_records = AbslHashMapFromObjCTxtRecords(txtRecordData);
          nsd_service_info.SetTxtRecords(txt_records);
        }
        connection_requesters_.insert({CppStringFromObjCString(serviceType), requestConnection});
        callback.service_discovered_cb(nsd_service_info);
        return ^{
          callback.service_lost_cb(nsd_service_info);
        };
      }];
  {
    absl::MutexLock lock(&mutex_);
    discovering_info_.Add(service_type, browser);
  }
  return true;
}

bool WifiLanMedium::StopDiscovery(const std::string& service_type) {
  {
    absl::MutexLock lock(&mutex_);
    if (!discovering_info_.Existed(service_type)) {
      GTMLoggerInfo(@"[NEARBY] WifiLan StopDiscovery: Can't stop discovering because "
                     "we never started discovering.");
      return false;
    }
    discovering_info_.Remove(service_type);
  }
  return true;
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const NsdServiceInfo& remote_service_info, CancellationFlag* cancellation_flag) {
  std::string service_type = remote_service_info.GetServiceType();
  GTMLoggerInfo(@"[NEARBY] WifiLan ConnectToService, service_type=%@",
                ObjCStringFromCppString(service_type));

  GNCMConnectionRequester connection_requester = nil;
  {
    absl::MutexLock lock(&mutex_);
    const auto& it = connection_requesters_.find(service_type);
    if (it == connection_requesters_.end()) {
      return {};
    }
    connection_requester = it->second;
  }

  dispatch_group_t group = dispatch_group_create();
  dispatch_group_enter(group);
  __block std::unique_ptr<WifiLanSocket> socket;
  if (connection_requester != nil) {
    if (cancellation_flag->Cancelled()) {
      GTMLoggerError(@"[NEARBY] WifiLan Connect: Has been cancelled: service_type=%@",
                     ObjCStringFromCppString(service_type));
      dispatch_group_leave(group);  // unblock
      return {};
    }

    connection_requester(^(id<GNCMConnection> connection) {
      // If the connection wasn't successfully established, return a NULL socket.
      if (connection) {
        socket = absl::make_unique<WifiLanSocket>(connection);
      }

      dispatch_group_leave(group);  // unblock
      return socket != nullptr ? static_cast<WifiLanInputStream&>(socket->GetInputStream())
                                     .GetConnectionHandlers()
                               : nullptr;
    });
  }
  dispatch_group_wait(group, DISPATCH_TIME_FOREVER);

  return std::move(socket);
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const std::string& ip_address, int port, CancellationFlag* cancellation_flag) {
  // Not implemented.
  return {};
}

std::unique_ptr<api::WifiLanServerSocket> WifiLanMedium::ListenForService(int port) {
  auto server_socket = std::make_unique<WifiLanServerSocket>();
  // The fake ip address and port need to be set here since they can't be retrieved before
  // StartAadvertising begins. Furthermore, NSNetService can't resolve ip address when finding
  // service. Try to fake them and make it socket name as a key of server_sockets_ which acts
  // the same socket binding.
  server_socket->SetIPAddress(GetFakeIPAddress());
  requesting_port_ = port;
  server_socket->SetPort(requesting_port_ == 0 ? GetFakePort() : requesting_port_);
  std::string socket_name =
      WifiLanServerSocket::GetName(server_socket->GetIPAddress(), server_socket->GetPort());
  server_socket->SetCloseNotifier([this, socket_name]() {
    absl::MutexLock lock(&mutex_);
    server_sockets_.erase(socket_name);
  });
  GTMLoggerInfo(@"[NEARBY] WifiLan Adding server socket, socket_name=%@",
                ObjCStringFromCppString(socket_name));
  absl::MutexLock lock(&mutex_);
  server_sockets_.insert({socket_name, server_socket.get()});
  return server_socket;
}

std::string WifiLanMedium::GetFakeIPAddress() const {
  std::string ip_address;
  ip_address.resize(4);
  uint32_t raw_ip_addr = Prng().NextUint32();
  ip_address[0] = static_cast<char>(raw_ip_addr >> 24);
  ip_address[1] = static_cast<char>(raw_ip_addr >> 16);
  ip_address[2] = static_cast<char>(raw_ip_addr >> 8);
  ip_address[3] = static_cast<char>(raw_ip_addr >> 0);

  return ip_address;
}

int WifiLanMedium::GetFakePort() const {
  uint16_t port = Prng().NextUint32();

  return port;
}

}  // namespace ios
}  // namespace nearby
}  // namespace location
