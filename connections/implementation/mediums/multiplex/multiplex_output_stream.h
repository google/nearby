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

#ifndef CORE_INTERNAL_MEDIUMS_MULTIPLEX_MULTIPLEX_OUTPUT_STREAM_H_
#define CORE_INTERNAL_MEDIUMS_MULTIPLEX_MULTIPLEX_OUTPUT_STREAM_H_

#include <memory>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "internal/platform/array_blocking_queue.h"
#include "internal/platform/atomic_boolean.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/exception.h"
#include "internal/platform/feature_flags.h"
#include "internal/platform/future.h"
#include "internal/platform/mutex.h"
#include "internal/platform/output_stream.h"
#include "internal/platform/single_thread_executor.h"
#include "proto/mediums/multiplex_frames.pb.h"

namespace nearby {
namespace connections {
namespace mediums {
namespace multiplex {
/**
 * A helper class to send out the {@code MultiplexControlFrame} and the outgoing
 * data from clients. It schedules control and data frames with priority below
 *
 * <p>{@link MultiplexControlFrameType#CONNECTION_REQUEST} and {@link
 * MultiplexControlFrameType#CONNECTION_RESPONSE} have the highest priority
 *
 * <p>All {@link MultiplexDataFrame} has the medium priority. If there's
 * multiple clients send data at the same time, should poll every client's
 * outgoing data in sequence. For example, client A and B send data at the same
 * time, the outgoing data sequence should like A-Frame-1, B-Frame-1, A-Frame-2,
 * B-Frame-2,...
 *
 * <p>{@link MultiplexControlFrameType#DISCONNECTION} has the same priority with
 * {@link MultiplexDataFrame} because the disconnect should not make the already
 * enqueued data failed to send out, so put it in the same priority queue with
 * the MultiplexDataFrame.
 */
class MultiplexOutputStream {
 public:
  enum class VirtualOutputStreamType {
    // The type of virtual socket established for the physical socket is
    // created.
    kFirstVirtualSocket = 0,
    // The others except FIRST_VIRTUAL_SCOKET type.
    kNormalVirtualSocket = 1,
  };

  MultiplexOutputStream(OutputStream* physical_writer,
                        AtomicBoolean& is_enabled);
  ~MultiplexOutputStream() = default;

  // Writes the connection request frame to the physical output stream.
  bool WriteConnectionRequestFrame(const std::string& service_id,
                                   const std::string& service_id_hash_salt);

  // Writes the connection response frame to the physical output stream.
  bool WriteConnectionResponseFrame(
      const ByteArray& salted_service_id_hash,
      const std::string& service_id_hash_salt,
      ::location::nearby::mediums::ConnectionResponseFrame::
          ConnectionResponseCode response_code);

  // Closes the virtual output stream.
  bool Close(const std::string& service_id);

  // Closes all virtual output streams.
  void CloseAll();

  // Waits for the result of the future.
  Exception WaitForResult(const std::string& method_name, Future<bool>* future);

  // Creates the virtual output stream for the first virtual socket.
  OutputStream* CreateVirtualOutputStreamForFirstVirtualSocket(
      const std::string& service_id, const std::string& service_id_hash_salt);

  // Creates the virtual output stream.
  OutputStream* CreateVirtualOutputStream(
      const std::string& service_id, const std::string& service_id_hash_salt);

  // Gets the service id hash salt.
  std::string GetServiceIdHashSalt(const std::string& service_id);

  // Shuts down the multiplex output stream.
  void Shutdown();

  class EnqueuedFrame {
   public:
    EnqueuedFrame(Future<bool>* future, ByteArray data)
        : future_(future), data_(data) {}
    ~EnqueuedFrame() = default;

    Future<bool>* future_;
    ByteArray data_;
  };

  class MultiplexWriter {
   public:
    explicit MultiplexWriter(OutputStream* physical_writer);
    ~MultiplexWriter();

    // Enqueues the frame to be sent out.
    void EnqueueToSend(Future<bool>* future, const ByteArray& data,
                       const std::string& frame_name);
    // Closes the writer.
    void Close();

   private:
    // Starts the writer thread.
    void StartWriting();

    // Writes the enqueued frame.
    void Write(EnqueuedFrame& enqueued_frame);

    Mutex writer_mutex_;
    OutputStream* physical_writer_ ABSL_PT_GUARDED_BY(writer_mutex_);

    ArrayBlockingQueue<EnqueuedFrame> data_queue_{
        FeatureFlags::GetInstance()
            .GetFlags()
            .multiplex_socket_middle_priority_queue_capacity};
    mutable Mutex writing_mutex_;
    ConditionVariable is_writing_cond_{&writing_mutex_};
    bool is_writing_ ABSL_GUARDED_BY(writing_mutex_) = false;
    bool is_closed_ = false;
    mutable Mutex close_writing_thread_mutex_;
    ConditionVariable close_writing_thread_cond_{&close_writing_thread_mutex_};

    // The single thread to write all enqueued frames.
    SingleThreadExecutor writer_thread_;
    bool is_write_loop_running_ = false;
  };

  class VirtualOutputStream : public OutputStream {
   public:
    VirtualOutputStream(std::string service_id,
                        std::string service_id_hash_salt,
                        OutputStream* physical_writer,
                        MultiplexWriter& multiplex_writer,
                        VirtualOutputStreamType virtual_output_stream_type,
                        MultiplexOutputStream& multiplex_output_stream);
    ~VirtualOutputStream() override = default;

    // Returns true if the virtual output stream is the first virtual output
    // stream.
    bool IsFirstVirtualOutputStream() {
      return virtual_output_stream_type_ ==
             VirtualOutputStreamType::kFirstVirtualSocket;
    }

    // Returns the service id hash salt.
    std::string GetServiceIdHashSalt() { return service_id_hash_salt_; }

    // Sets the service id hash salt.
    void SetserviceIdHashSalt(std::string service_id_hash_salt) {
      service_id_hash_salt_ = service_id_hash_salt;
    }

    // Writes the data to the physical output stream.
    Exception Write(const ByteArray& data) override;
    // Flushes the physical output stream.
    Exception Flush() override;
    // Closes the virtual output stream.
    Exception Close() override;

   private:
    AtomicBoolean is_closed_{false};

    std::string service_id_;
    std::string service_id_hash_salt_;
    OutputStream* physical_writer_;
    MultiplexWriter& multiplex_writer_;
    VirtualOutputStreamType virtual_output_stream_type_;
    bool first_frame_sent_for_first_virtual_output_stream_ = false;
    MultiplexOutputStream& multiplex_output_stream_;
  };

 private:
  AtomicBoolean& is_enabled_;
  OutputStream* physical_writer_;
  absl::flat_hash_map<std::string, std::unique_ptr<VirtualOutputStream>>
      virtual_output_streams_;
  MultiplexWriter multiplex_writer_;
};

}  // namespace multiplex
}  // namespace mediums
}  // namespace connections
}  // namespace nearby

#endif  // CORE_INTERNAL_MEDIUMS_MULTIPLEX_MULTIPLEX_OUTPUT_STREAM_H_
