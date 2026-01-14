// Copyright 2024 Google LLC
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

#include "internal/platform/implementation/linux/ble_l2cap_server_socket.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "internal/platform/logging.h"
#include "internal/platform/prng.h"

namespace nearby {
namespace linux {

BleL2capServerSocket::BleL2capServerSocket() : psm_(0) {}

BleL2capServerSocket::BleL2capServerSocket(int psm) : psm_(psm) {
}

BleL2capServerSocket::~BleL2capServerSocket() { Close(); }

void BleL2capServerSocket::SetPSM(int psm) { psm_ = psm; }

std::unique_ptr<api::ble_v2::BleL2capSocket> BleL2capServerSocket::Accept() {
  if (stopped_.Cancelled()) {
    LOG(ERROR) << __func__ << ": server socket has been stopped";
    return nullptr;
  }

  absl::MutexLock lock(&mutex_);

  Prng prng;
  psm_ = 0x80 + (prng.NextUint32() % 0x80);

  server_fd_ = socket(AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
  int rcv = 1 << 20;  // 1 MiB (kernel may clamp)
  int snd = 1 << 20;
  int err2 = setsockopt(server_fd_, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof(rcv));
  int err3 = setsockopt(server_fd_, SOL_SOCKET, SO_SNDBUF, &snd, sizeof(snd));

  LOG(INFO) << __func__ << ": Using server_fd: " << server_fd_;
  if (server_fd_ < 0 or err2 == -1  or err3 == -1) {
    LOG(ERROR) << "Failed to create L2CAP server socket: "
               << std::strerror(errno);
    return nullptr;
  }

  struct sockaddr_l2 addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.l2_family = AF_BLUETOOTH;
  addr.l2_psm = htobs(psm_);
  addr.l2_cid = 0;
  addr.l2_bdaddr_type = BDADDR_LE_PUBLIC;
  // Set BDADDR_ANY (all zeros)
  std::memset(&addr.l2_bdaddr, 0, sizeof(addr.l2_bdaddr));

  if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    LOG(ERROR) << "Failed to bind L2CAP server socket: "
               << std::strerror(errno) << " (errno: " << errno << ")";
    close(server_fd_);
    server_fd_ = -1;
    return nullptr;
  }

  if (listen(server_fd_, 5) < 0) {
    LOG(ERROR) << "Failed to listen on L2CAP server socket: "
               << std::strerror(errno);
    close(server_fd_);
    server_fd_ = -1;
    return nullptr;
  }

  socklen_t addr_len = sizeof(addr);
  if (getsockname(server_fd_, (struct sockaddr*)&addr, &addr_len) == 0) {
    psm_ = btohs(addr.l2_psm);
    LOG(INFO) << "L2CAP server socket listening on PSM: " << psm_;
  } else {
    LOG(ERROR) << "Failed to get socket name: " << std::strerror(errno);
  }
  // // Return cached socket if it exists
  // auto it = accepted_fds_.find(server_fd_);
  // if (it != accepted_fds_.end()) {
  //   LOG(INFO) << __func__ << ": Socket exists. Returning cached socket";
  //   return std::make_unique<BleL2capSocket>(it->second.first, it->second.second); // I hate how this looks
  // }

  if (server_fd_ < 0) {
    LOG(ERROR) << "Server socket not initialized";
    return nullptr;
  }

  // Release the mutex while waiting for incoming connection
  mutex_.Unlock();

  struct sockaddr_l2 client_addr;
  socklen_t client_len = sizeof(client_addr);
  std::memset(&client_addr, 0, sizeof(client_addr));

  LOG(INFO) << "Waiting for L2CAP connection on PSM " << psm_ << "...";
  int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
  
  // Re-acquire the mutex
  mutex_.Lock();

  if (client_fd < 0) {
    if (errno == EINTR || errno == EAGAIN) {
      LOG(WARNING) << "Accept interrupted, returning nullptr";
      return nullptr;
    }
    LOG(ERROR) << "Failed to accept L2CAP connection: " << std::strerror(errno);
    return nullptr;
  }

  if (closed_) {
    close(client_fd);
    return nullptr;
  }

  char client_addr_str[18];
  ba2str(&client_addr.l2_bdaddr, client_addr_str);
  LOG(INFO) << "Accepted L2CAP connection from " << client_addr_str 
            << " on PSM " << btohs(client_addr.l2_psm);

  LOG(INFO) << __func__ << ": Connected to client_fd: " << client_fd;
  // Create a unique ID from the MAC address
  api::ble_v2::BlePeripheral::UniqueId peripheral_id = 0;
  for (int i = 0; i < 6; i++) {
    peripheral_id = (peripheral_id << 8) | client_addr.l2_bdaddr.b[i];
  }

  accepted_fds_.emplace(server_fd_, std::pair(client_fd, peripheral_id));
  return std::make_unique<BleL2capSocket>(client_fd, peripheral_id);
}

Exception BleL2capServerSocket::Close() {
  LOG(ERROR) << __func__ << ": closing bluetooth server socket";
  stopped_.Cancel();

  return DoClose();
}

Exception BleL2capServerSocket::DoClose() {
  closed_ = true;
  
  if (server_fd_ >= 0) {
    shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_);
    server_fd_ = -1;
  }
  
  if (close_notifier_) {
    auto notifier = std::move(close_notifier_);
    mutex_.Unlock();
    notifier();
    mutex_.Lock();
  }
  
  return {Exception::kSuccess};
}

void BleL2capServerSocket::SetCloseNotifier(
    absl::AnyInvocable<void()> notifier) {
  absl::MutexLock lock(&mutex_);
  close_notifier_ = std::move(notifier);
}

}  // namespace linux
}  // namespace nearby
