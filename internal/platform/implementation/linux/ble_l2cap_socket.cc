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

#include "internal/platform/implementation/linux/ble_l2cap_socket.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <bluetooth/bluetooth.h>

#include "absl/strings/escaping.h"
#include "absl/strings/string_view.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/logging.h"
#include <sys/ioctl.h>

#include "bluetooth_classic_socket.h"

namespace nearby {
namespace linux {

namespace {
constexpr size_t kLogMaxBytes = 64;
constexpr size_t kDefaultBleL2capMtu = 23;

size_t GetSocketMtu(int fd, int option_name) {
  uint16_t mtu = 0;
  socklen_t len = sizeof(mtu);
  if (getsockopt(fd, SOL_BLUETOOTH, option_name, &mtu, &len) == 0 &&
      mtu > 0) {
    return static_cast<size_t>(mtu);
  }
  return 0;
}

std::string HexPreview(const char* data, size_t size) {
  size_t count = std::min(size, kLogMaxBytes);
  std::string hex =
      absl::BytesToHexString(absl::string_view(data, count));
  if (size > count) {
    hex.append("...");
  }
  return hex;
}

size_t GetL2capOutputMtu(int fd) {
  struct l2cap_options opts;
  size_t mtu = GetSocketMtu(fd, BT_SNDMTU);
  if (mtu > 0) {
    return mtu;
  }
  return kDefaultBleL2capMtu;
}

Exception PollSocket(int fd, short event) {
  struct pollfd fds[1];
  fds[0].fd = fd;
  fds[0].events = event;

  while (true) {
    int ret = poll(fds, 1, -1);
    if (ret < 0) {
      if (errno == EINTR) continue;
      LOG(ERROR) << "Error polling L2CAP socket: " << std::strerror(errno);
      return {Exception::kIo};
    }
    if ((fds[0].revents & event) != 0) {
      return {Exception::kSuccess};
    }
    if ((fds[0].revents & POLLHUP) != 0) {
      LOG(ERROR) << "L2CAP socket disconnected";
      return {Exception::kIo};
    }
    if ((fds[0].revents & (POLLERR | POLLNVAL)) != 0) {
      LOG(ERROR) << "Error occurred on L2CAP socket";
      return {Exception::kIo};
    }
  }
}
}  // namespace

BleL2capInputStream::BleL2capInputStream(int fd) : fd_(fd) {}

BleL2capInputStream::~BleL2capInputStream() { Close(); }

static size_t GetBleCocRcvMtu(int fd) {
  // Prefer BT_RCVMTU for LE CoC, but fall back to L2CAP_OPTIONS if needed.
  uint16_t mtu = 0;
  socklen_t len = sizeof(mtu);
  if (getsockopt(fd, SOL_BLUETOOTH, BT_RCVMTU, &mtu, &len) == 0 && mtu > 0)
    return mtu;

  // Fallback ONLY (avoid 23 unless you're sure; 23 causes truncation if peer sends bigger SDUs).
  return 512;
}

  static size_t NextPacketSize(int fd) {
  int pending = 0;
  if (ioctl(fd, FIONREAD, &pending) == 0 && pending > 0) {
    return static_cast<size_t>(pending);
  }
  return 0;  // unknown
}
ExceptionOr<ByteArray> BleL2capInputStream::Read(std::int64_t size) {
  int fd = fd_.load();
  if (fd < 0) return Exception{Exception::kIo};

  auto poller = Poller::CreateInputPoller(fd);

    // Wait for readability
    while (true) {
      if (fd_.load() != fd) return {Exception::kIo};
      auto result = poller.Ready();
      if (result.Raised()) return result;
      if (fd_.load() != fd) return {Exception::kIo};

      // Peek the next message length without consuming it.
      // For seqpacket/dgram, MSG_TRUNC makes recv() return the *full* message length
      // even if the buffer is smaller.
      ssize_t msg_len = ::recv(fd, nullptr, 0, MSG_PEEK | MSG_TRUNC);
      if (msg_len < 0) {
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
        if (errno == EBADF) {
          LOG(INFO) << __func__ << ": socket was closed during read";
          return {Exception::kIo};
        }
        LOG(ERROR) << __func__ << ": error peeking message length: "
                   << std::strerror(errno);
        return {Exception::kIo};
      }
      if (msg_len == 0) {
        LOG(INFO) << __func__ << ": socket closed (EOF)";
        return {Exception::kIo};
      }

      // Decide how much we will actually read/return.
      // If caller asked for 'size', cap to that.
      size_t want = static_cast<size_t>(msg_len);
      size_t cap  = static_cast<size_t>(size);
      size_t to_read = std::min(want, cap);

      std::string buffer;
      buffer.resize(to_read);

      // Now read/consume the message. If the message is larger than to_read,
      // the remainder will be discarded by the kernel for seqpacket/dgram.
      // We can detect that and treat it as an error (or choose a different policy).
      if (fd_.load() != fd) return {Exception::kIo};
      ssize_t n = ::recv(fd, buffer.data(), to_read, 0);
      if (n < 0) {
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
        if (errno == EBADF) {
          LOG(INFO) << __func__ << ": socket was closed during read";
          return {Exception::kIo};
        }
        LOG(ERROR) << __func__ << ": error reading data on bluetooth socket: "
                   << std::strerror(errno);
        return {Exception::kIo};
      }
      if (n == 0) {
        LOG(INFO) << __func__ << ": socket closed (EOF)";
        return {Exception::kIo};
      }

      buffer.resize(static_cast<size_t>(n));

      // Detect truncation: if msg_len > size, we truncated/discarded remainder.
      if (want > cap) {
        LOG(ERROR) << __func__
                   << ": incoming packet (" << want
                   << " bytes) exceeds requested size (" << cap
                   << "). Packet truncated.";
        return {Exception::kIo};
      }

      return ExceptionOr{ByteArray(std::move(buffer))};
    }
}

Exception BleL2capInputStream::Close() {
  int fd = fd_.exchange(-1);
  if (fd < 0) return {Exception::kSuccess};  // Already closed
  ::shutdown(fd, SHUT_RDWR);
  return {Exception::kSuccess};
}

BleL2capOutputStream::BleL2capOutputStream(int fd) : fd_(fd) {}

BleL2capOutputStream::~BleL2capOutputStream() { Close(); }

Exception BleL2capOutputStream::Write(const ByteArray& data) {
  int fd = fd_.load();
  if (fd < 0) return Exception{Exception::kIo};

  auto poller = Poller::CreateOutputPoller(fd);

  size_t total_wrote = 0;

  if (data.Empty()) {
    return {Exception::kSuccess};
  }
  size_t max_chunk_size = GetL2capOutputMtu(fd);
  if (max_chunk_size == 0) {
    max_chunk_size = kDefaultBleL2capMtu;
  }
  LOG(INFO) << "BleL2capOutputStream::Write bytes=" << data.size()
            << " mtu=" << max_chunk_size << " data=0x"
            << HexPreview(data.data(), data.size());
 while (total_wrote < data.size()) {
    if (fd_.load() != fd) return {Exception::kIo};
    auto result = poller.Ready();  // should wait for POLLOUT/EPOLLOUT
    if (result.Raised()) return result;
    if (fd_.load() != fd) return {Exception::kIo};

    const char *buf = data.data();
    size_t remaining = data.size() - total_wrote;

    size_t to_write = remaining;
    {
      // For SEQPACKET/DGRAM, one send() == one packet.
      // Cap to discovered “MTU-like” limit to avoid EMSGSIZE.
      absl::MutexLock lock(&fd_mutex_);
      to_write = std::min(to_write, max_chunk_size);
    }

    // Prefer send() to avoid SIGPIPE (MSG_NOSIGNAL is Linux).
    ssize_t wrote = ::send(fd,
                           buf + total_wrote,
                           to_write,
#ifdef MSG_NOSIGNAL
                           MSG_NOSIGNAL
#else
                           0
#endif
    );

    // If send() isn’t appropriate in your environment, you can swap back to write().
    // ssize_t wrote = ::write(fd_.get(), buf + total_wrote, to_write);

    if (wrote < 0) {
      if (errno == EINTR) continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK) continue;

      if (errno == EMSGSIZE) {
        // Our packet is too large; shrink max_chunk_ and retry.
        {
          absl::MutexLock lock(&fd_mutex_);
          if (max_chunk_size > 1) {
            max_chunk_size = std::max<size_t>(1, max_chunk_size / 2);
            LOG(INFO) << __func__ << ": EMSGSIZE; reducing max_chunk_ to "
                      << max_chunk_size;
            continue;  // retry with smaller chunk
          }
        }
        LOG(ERROR) << __func__ << ": EMSGSIZE even at 1 byte";
        return {Exception::kIo};
      }

      if (errno == EBADF || errno == EPIPE) {
        LOG(INFO) << __func__ << ": socket was closed during write";
        return {Exception::kIo};
      }

      LOG(ERROR) << __func__
                 << ": error writing data on bluetooth socket: "
                 << std::strerror(errno);
      return {Exception::kIo};
    }

    if (wrote == 0) {
      // For sockets, 0 usually means peer closed.
      LOG(INFO) << __func__ << ": peer closed during write";
      return {Exception::kIo};
    }

    total_wrote += static_cast<size_t>(wrote);
  }

  return {Exception::kSuccess};
}

Exception BleL2capOutputStream::Close() {
  int fd = fd_.exchange(-1);
  if (fd < 0) return {Exception::kSuccess};  // Already closed
  ::shutdown(fd, SHUT_RDWR);
  return {Exception::kSuccess};
}

BleL2capSocket::BleL2capSocket(int fd,
                               api::ble_v2::BlePeripheral::UniqueId peripheral_id)
    : peripheral_id_(peripheral_id),
      input_stream_(std::make_unique<BleL2capInputStream>(fd)),
      output_stream_(std::make_unique<BleL2capOutputStream>(fd))
{
  LOG(INFO) << "fd_ " << fd;
  LOG(INFO) << "input_stream_ :" << input_stream_.get();
  LOG(INFO) << "output_stream_ :" << output_stream_.get();
  struct l2cap_options opts;
  size_t snd_mtu = GetSocketMtu(fd, BT_SNDMTU);
  size_t rcv_mtu = GetSocketMtu(fd, BT_RCVMTU);
  LOG(INFO) << "BleL2capSocket MTU fallback snd_mtu=" << snd_mtu
            << " rcv_mtu=" << rcv_mtu;
}

BleL2capSocket::~BleL2capSocket() { Close(); }

Exception BleL2capSocket::Close() {
  absl::MutexLock lock(&mutex_);
  if (closed_) {
    return {Exception::kSuccess};
  }
  DoClose();
  return {Exception::kSuccess};
}

void BleL2capSocket::DoClose() {
  closed_ = true;
  
  if (input_stream_) {
    input_stream_->Close();
  }
  if (output_stream_) {
    output_stream_->Close();
  }
  
  if (close_notifier_) {
    auto notifier = std::move(close_notifier_);
    mutex_.Unlock();
    notifier();
    mutex_.Lock();
  }
}

void BleL2capSocket::SetCloseNotifier(absl::AnyInvocable<void()> notifier) {
  absl::MutexLock lock(&mutex_);
  close_notifier_ = std::move(notifier);
}

bool BleL2capSocket::IsClosed() const {
  absl::MutexLock lock(&mutex_);
  return closed_;
}

}  // namespace linux
}  // namespace nearby
