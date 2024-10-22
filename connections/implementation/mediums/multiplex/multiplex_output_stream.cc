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

#include "connections/implementation/mediums/multiplex/multiplex_output_stream.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "connections/implementation/mediums/multiplex/multiplex_frames.h"
#include "internal/platform/array_blocking_queue.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/base64_utils.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/future.h"
#include "internal/platform/logging.h"
#include "internal/platform/mutex_lock.h"
#include "internal/platform/output_stream.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace multiplex {
namespace {
using ::location::nearby::mediums::ConnectionResponseFrame;

constexpr absl::string_view kFakeSalt = "RECEIVER_CONDIMENT";
}  // namespace

// Implementation for class MultiplexOutputStream
MultiplexOutputStream::MultiplexOutputStream(OutputStream* physical_writer,
                                             AtomicBoolean& is_enabled)
    : is_enabled_(is_enabled),
      physical_writer_(physical_writer),
      multiplex_writer_{physical_writer} {}

Exception MultiplexOutputStream::WaitForResult(const std::string& method_name,
                                               Future<bool>* future) {
  if (!future) {
    NEARBY_LOGS(INFO) << "No future to wait for; return with error.";
    return {Exception::kFailed};
  }
  NEARBY_LOGS(INFO) << "Waiting for future to complete: " << method_name;
  ExceptionOr<bool> result =
      future->Get(FeatureFlags::GetInstance()
                      .GetFlags()
                      .mediums_frame_write_timeout_millis);
  if (!result.ok()) {
    NEARBY_LOGS(INFO) << "Future:[" << method_name
                      << "] completed with exception:" << result.exception();
    return {Exception::kFailed};
  }
  if (result.result()) {
    NEARBY_LOGS(INFO) << "Future:[" << method_name
                      << "] completed with success.";
    return {Exception::kSuccess};
  }
  NEARBY_LOGS(INFO) << "Future:[" << method_name
                    << "] completed with failure.";
  return {Exception::kFailed};
}

bool MultiplexOutputStream::WriteConnectionRequestFrame(
    const std::string& service_id, const std::string& service_id_hash_salt) {
  if (!is_enabled_.Get()) {
    return false;
  }
  Future<bool> future;
  multiplex_writer_.EnqueueToSend(
      &future, ForConnectionRequest(service_id, service_id_hash_salt),
      "MultiplexFrame::CONNECTION_REQUEST");
  if (WaitForResult("MultiplexFrame::CONNECTION_REQUEST", &future).Ok())
    return true;
  return false;
}

bool MultiplexOutputStream::WriteConnectionResponseFrame(
    const ByteArray& salted_service_id_hash,
    const std::string& service_id_hash_salt,
    ConnectionResponseFrame::ConnectionResponseCode response_code) {
  if (!is_enabled_.Get()) {
    return false;
  }
  Future<bool> future;
  multiplex_writer_.EnqueueToSend(
      &future,
      ForConnectionResponse(salted_service_id_hash, service_id_hash_salt,
                            response_code),
      "MultiplexFrame::CONNECTION_RESPONSE");
  if (WaitForResult("MultiplexFrame::CONNECTION_RESPONSE", &future).Ok())
    return true;
  return false;
}

bool MultiplexOutputStream::Close(const std::string& service_id) {
  auto item = virtual_output_streams_.find(service_id);
  if (item == virtual_output_streams_.end()) {
    NEARBY_LOGS(INFO) << "Don't need to close VirtualOutputStream("
                         << service_id << ") because it's already gone.";
    return false;
  }

  item->second->Close();
  if (is_enabled_.Get()) {
    Future<bool> future;
    multiplex_writer_.EnqueueToSend(
        &future,
        ForDisconnection(service_id, item->second->GetServiceIdHashSalt()),
        "MultiplexFrame::DISCONNECTION");
    WaitForResult("MultiplexFrame::DISCONNECTION", &future);
  }
  virtual_output_streams_.erase(service_id);
  if (virtual_output_streams_.empty()) {
    physical_writer_->Close();
    multiplex_writer_.Close();
  }
  return true;
}

void MultiplexOutputStream::CloseAll() {
  for (auto& [service_id, virtual_output_stream] : virtual_output_streams_) {
    if (is_enabled_.Get()) {
      Future<bool> future;
      multiplex_writer_.EnqueueToSend(
          &future,
          ForDisconnection(service_id,
                           virtual_output_stream->GetServiceIdHashSalt()),
          "MultiplexFrame::DISCONNECTION");
      WaitForResult("MultiplexFrame::DISCONNECTION", &future);
    }
    virtual_output_stream->Close();
  }
  virtual_output_streams_.clear();
  physical_writer_->Close();
  multiplex_writer_.Close();
}

OutputStream*
MultiplexOutputStream::CreateVirtualOutputStreamForFirstVirtualSocket(
    const std::string& service_id, const std::string& service_id_hash_salt) {
  return virtual_output_streams_
      .emplace(service_id,
               std::make_unique<VirtualOutputStream>(
                   service_id, service_id_hash_salt, physical_writer_,
                   multiplex_writer_,
                   VirtualOutputStreamType::kFirstVirtualSocket, *this))
      .first->second.get();
}

OutputStream* MultiplexOutputStream::CreateVirtualOutputStream(
    const std::string& service_id, const std::string& service_id_hash_salt) {
  return virtual_output_streams_
      .emplace(service_id,
               std::make_unique<VirtualOutputStream>(
                   service_id, service_id_hash_salt, physical_writer_,
                   multiplex_writer_,
                   VirtualOutputStreamType::kNormalVirtualSocket, *this))
      .first->second.get();
}

std::string MultiplexOutputStream::GetServiceIdHashSalt(
    const std::string& service_id) {
  auto item = virtual_output_streams_.find(service_id);
  if (item != virtual_output_streams_.end()) {
    return item->second->GetServiceIdHashSalt();
  }
  return {};
}

void MultiplexOutputStream::Shutdown() {
  physical_writer_->Close();
  multiplex_writer_.Close();
}

// Implementation for class MultiplexOutputStream::MultiplexWriter
MultiplexOutputStream::MultiplexWriter::MultiplexWriter(
    OutputStream* physical_writer)
    : physical_writer_(physical_writer) {}

MultiplexOutputStream::MultiplexWriter::~MultiplexWriter() {
  Close();
  physical_writer_ = nullptr;
}

void MultiplexOutputStream::MultiplexWriter::EnqueueToSend(
    Future<bool>* future, const ByteArray& data,
    const std::string& frame_name) {
  MutexLock lock(&writing_mutex_);
  data_queue_.Put(EnqueuedFrame(future, data));

  if (is_writing_) {
    return;
  }
  is_writing_ = true;
  is_writing_cond_.Notify();
  if (!is_write_loop_running_) {
    is_write_loop_running_ = true;
    writer_thread_.Execute("Start writing", [this] { StartWriting(); });
  }
}

void MultiplexOutputStream::MultiplexWriter::StartWriting() {
  NEARBY_LOGS(INFO) << "Writing loop started.";
  while (true) {
    auto enqueued_frame = data_queue_.TryTake();
    if (enqueued_frame != std::nullopt) {
      Write(enqueued_frame.value());
      continue;
    }
    {
      MutexLock lock(&writing_mutex_);
      if (data_queue_.Empty() && is_writing_ && !is_closed_) {
        is_writing_ = false;
        NEARBY_LOGS(INFO) << "Waiting for data_queue_ has data.";
        Exception wait_succeeded = is_writing_cond_.Wait();
        if (!wait_succeeded.Ok()) {
          NEARBY_LOGS(WARNING)
              << "Failure waiting to wait: " << wait_succeeded.value;
          return;
        }
      }
      if (is_closed_) {
        NEARBY_LOGS(INFO) << "Notify to close_writing_thread";
        MutexLock lock(&close_writing_thread_mutex_);
        close_writing_thread_cond_.Notify();
        break;
      }
    }
  }
  NEARBY_LOGS(INFO) << "Writing loop stopped.";
}

void MultiplexOutputStream::MultiplexWriter::Write(
    EnqueuedFrame& enqueued_frame) {
  MutexLock lock(&writer_mutex_);
  if (!physical_writer_
           ->Write(Base64Utils::IntToBytes(enqueued_frame.data_.size()))
           .Ok()) {
    enqueued_frame.future_->SetException({Exception::kIo});
    return;
  };
  if (!physical_writer_->Write(enqueued_frame.data_).Ok()) {
    enqueued_frame.future_->SetException({Exception::kIo});
    return;
  };
  if (!physical_writer_->Flush().Ok()) {
    enqueued_frame.future_->SetException({Exception::kIo});
    return;
  };
  enqueued_frame.future_->Set(true);
}

void MultiplexOutputStream::MultiplexWriter::Close() {
  if (is_closed_) {
    NEARBY_LOGS(INFO) << "MultiplexWriter is already closed.";
    return;
  }
  NEARBY_LOGS(INFO) << "Stop writing loop and Shutdown writer thread.";
  {
    MutexLock lock(&writing_mutex_);
    is_closed_ = true;
    if (!is_write_loop_running_) {
      writer_thread_.Shutdown();
      return;
    }
    is_write_loop_running_ = false;
    is_writing_cond_.Notify();
  }
  NEARBY_LOGS(INFO) << "Wait to close_writing_thread";
  {
    MutexLock lock(&close_writing_thread_mutex_);
    close_writing_thread_cond_.Wait(absl::Milliseconds(20));
    NEARBY_LOGS(INFO) << "Shutdown writer thread.";
    writer_thread_.Shutdown();
  }
}

MultiplexOutputStream::VirtualOutputStream::VirtualOutputStream(
    std::string service_id, std::string service_id_hash_salt,
    OutputStream* physical_writer, MultiplexWriter& multiplex_writer,
    VirtualOutputStreamType virtual_output_stream_type,
    MultiplexOutputStream& multiplex_output_stream)
    : service_id_(service_id),
      service_id_hash_salt_(service_id_hash_salt),
      physical_writer_(physical_writer),
      multiplex_writer_(multiplex_writer),
      virtual_output_stream_type_(virtual_output_stream_type),
      multiplex_output_stream_(multiplex_output_stream) {}

Exception MultiplexOutputStream::VirtualOutputStream::Write(
    const ByteArray& data) {
  if (is_closed_.Get()) {
    NEARBY_LOGS(WARNING)
        << "Failed to write data because the VirtualOutputStream for "
        << service_id_ << " closed";
    return {Exception::kIo};
  }
  if (multiplex_output_stream_.is_enabled_.Get()) {
    bool should_pass_salt = false;
    if (IsFirstVirtualOutputStream()) {
      if (!first_frame_sent_for_first_virtual_output_stream_) {
        first_frame_sent_for_first_virtual_output_stream_ = true;
        should_pass_salt = true;
      }
      // Fixes b/290724590, b/290983930 which can't get the correct socket
      // from the virtualSockets map. NS receiver side will pass 2
      // DATA_FRAMEs continuously to the remote sender side but originally
      // impl will only consider the 1st one. Add below fix to handle 2nd
      // frame which the salt is still fake one and change shouldPassSalt to
      // true to let the remote handle correctly.
      if ((service_id_hash_salt_ == kFakeSalt) && !should_pass_salt) {
        should_pass_salt = true;
        NEARBY_LOGS(INFO) << "service_idHashSalt is still a fake one and "
                             "not changed yet; continue to pass salt.";
      }
    }
    ByteArray data_frame =
        ForData(service_id_, service_id_hash_salt_, should_pass_salt, data);
    Future<bool> future;
    multiplex_writer_.EnqueueToSend(&future, data_frame,
                                    "MultiplexFrame::DATA_FRAME");
    return multiplex_output_stream_.WaitForResult("MultiplexFrame::DATA_FRAME",
                                                  &future);
  } else {
    if (!physical_writer_->Write(data).Ok()) {
      return {Exception::kIo};
    };
    if (!physical_writer_->Flush().Ok()) {
      return {Exception::kIo};
    };
  }

  return {Exception::kSuccess};
}

Exception MultiplexOutputStream::VirtualOutputStream::Flush() {
  return {Exception::kSuccess};
}

Exception MultiplexOutputStream::VirtualOutputStream::Close() {
  NEARBY_LOGS(INFO) << "MultiplexOutputStream::VirtualOutputStream::Close";
  is_closed_.Set(true);
  return {Exception::kSuccess};
}

}  // namespace multiplex
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
