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

#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>

#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/implementation/linux/bluetooth_classic_socket.h"
#include "internal/platform/logging.h"

#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cstddef>
#include <algorithm>

struct SocketWriteCaps {
  int so_type = 0;         // SOCK_STREAM / SOCK_SEQPACKET / SOCK_DGRAM
  size_t max_chunk = 0;    // 0 => unknown/unlimited
  bool packet_based = false;
};

static SocketWriteCaps DetectCapsNoBtHeaders(int fd) {
  SocketWriteCaps caps{};

  socklen_t len = sizeof(caps.so_type);
  if (::getsockopt(fd, SOL_SOCKET, SO_TYPE, &caps.so_type, &len) != 0) {
    // If we can't detect, behave conservatively like stream.
    caps.so_type = SOCK_STREAM;
  }

  caps.packet_based = (caps.so_type == SOCK_SEQPACKET || caps.so_type == SOCK_DGRAM);

  // Initial guess for packet-based sockets. This will be refined on EMSGSIZE.
  if (caps.packet_based) caps.max_chunk = 1024; // start guess
  else caps.max_chunk = 0;                     // unlimited/stream

  return caps;
}

namespace nearby {
namespace linux {
Exception Poller::Ready() {
  while (true) {
    auto ret = poll(fds_, 1, -1);
    if (ret < 0) {
      if (errno == EAGAIN) continue;
      LOG(ERROR) << __func__ << ": error polling socket for I/O: "
                         << std::strerror(errno);
      return {Exception::kIo};
    }
    if ((fds_[0].revents & poll_event_) != 0) {
      return {Exception::kSuccess};
    }
    if ((fds_[0].revents & POLLHUP) != 0) {
      LOG(ERROR) << __func__ << ": socket disconnected";
      return {Exception::kIo};
    }
    if ((fds_[0].revents & (POLLERR | POLLNVAL)) != 0) {
      LOG(ERROR) << __func__ << ": an error occured on the socket";
      return {Exception::kIo};
    }
  }
}


ExceptionOr<ByteArray> BluetoothInputStream::Read(std::int64_t size) {
  absl::MutexLock lock(&fd_mutex_);
  if (!fd_.isValid()) return Exception{Exception::kIo};

  auto poller = Poller::CreateInputPoller(fd_);

  int so_type = 0;
  socklen_t sl = sizeof(so_type);
  if (::getsockopt(fd_.get(), SOL_SOCKET, SO_TYPE, &so_type, &sl) != 0) {
    // If unknown, default to stream-ish behavior.
    so_type = SOCK_STREAM;
  }
  const bool packet_based = (so_type == SOCK_SEQPACKET || so_type == SOCK_DGRAM);

  // Sanity: avoid negative / zero sizes
  if (size <= 0) return ExceptionOr{ByteArray(std::string())};

  // ---- Packet-based: read ONE message (recommended) ----
  if (packet_based) {
    // Wait for readability
    while (true) {
      auto result = poller.Ready();
      if (result.Raised()) return result;

      // Peek the next message length without consuming it.
      // For seqpacket/dgram, MSG_TRUNC makes recv() return the *full* message length
      // even if the buffer is smaller.
      ssize_t msg_len = ::recv(fd_.get(), nullptr, 0, MSG_PEEK | MSG_TRUNC);
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
      ssize_t n = ::recv(fd_.get(), buffer.data(), to_read, 0);
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

  // ---- Stream-based: read exactly 'size' bytes (your original behavior) ----
  std::string buffer;
  buffer.resize(static_cast<size_t>(size));
  char* data = buffer.data();

  size_t total_read = 0;
  while (total_read < static_cast<size_t>(size)) {
    auto result = poller.Ready();
    if (result.Raised()) return result;

    ssize_t bytes_read = ::read(fd_.get(),
                                data + total_read,
                                static_cast<size_t>(size) - total_read);
    if (bytes_read < 0) {
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
    if (bytes_read == 0) {
      LOG(INFO) << __func__ << ": socket closed (EOF)";
      return {Exception::kIo};
    }
    total_read += static_cast<size_t>(bytes_read);
  }

  return ExceptionOr{ByteArray(std::move(buffer))};
}

Exception BluetoothInputStream::Close() {
  absl::MutexLock lock(&fd_mutex_);
  if (!fd_.isValid()) return {Exception::kSuccess};  // Already closed
  fd_.reset();
  return {Exception::kSuccess};
}

Exception BluetoothOutputStream::Write(const ByteArray &data) {
  absl::MutexLock lock(&fd_mutex_);
  if (!fd_.isValid()) return Exception{Exception::kIo};

  auto poller = Poller::CreateOutputPoller(fd_);

  size_t total_wrote = 0;

  int so_type = 0;
  socklen_t sl = sizeof(so_type);
  if (::getsockopt(fd_.get(), SOL_SOCKET, SO_TYPE, &so_type, &sl) != 0) {
    // If we can’t detect, assume stream semantics (no per-message MTU).
    so_type = SOCK_STREAM;
  }

  const bool packet_based = (so_type == SOCK_SEQPACKET || so_type == SOCK_DGRAM);

  // Initialize a reasonable starting guess for packet-based sockets.
  // This will be refined down on EMSGSIZE.
  if (packet_based && max_chunk_ == 0) max_chunk_ = 1024;

  LOG(INFO) << "SO_TYPE=" << so_type
            << (so_type == SOCK_SEQPACKET ? " (SEQPACKET)" :
                so_type == SOCK_STREAM    ? " (STREAM)" :
                so_type == SOCK_DGRAM     ? " (DGRAM)" : " (other)")
            << (packet_based ? absl::StrFormat(" max_chunk=%zu", max_chunk_) : "");

  while (total_wrote < data.size()) {
    auto result = poller.Ready();  // should wait for POLLOUT/EPOLLOUT
    if (result.Raised()) return result;

    const char *buf = data.data();
    size_t remaining = data.size() - total_wrote;

    size_t to_write = remaining;
    if (packet_based) {
      // For SEQPACKET/DGRAM, one send() == one packet.
      // Cap to discovered “MTU-like” limit to avoid EMSGSIZE.
      to_write = std::min(to_write, max_chunk_);
    }

    // Prefer send() to avoid SIGPIPE (MSG_NOSIGNAL is Linux).
    ssize_t wrote = ::send(fd_.get(),
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

      if (errno == EMSGSIZE && packet_based) {
        // Our packet is too large; shrink max_chunk_ and retry.
        if (max_chunk_ > 1) {
          max_chunk_ = std::max<size_t>(1, max_chunk_ / 2);
          LOG(INFO) << __func__ << ": EMSGSIZE; reducing max_chunk_ to " << max_chunk_;
          continue;  // retry with smaller chunk
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

Exception BluetoothOutputStream::Close() {
  absl::MutexLock lock(&fd_mutex_);
  if (!fd_.isValid()) return {Exception::kSuccess};  // Already closed
  fd_.reset();
  return {Exception::kSuccess};
}

}  // namespace linux
}  // namespace nearby
