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

#include "platform/pipe.h"

#include <pthread.h>

#include <cstring>

#include "platform/api/platform.h"
#include "platform/port/string.h"
#include "platform/prng.h"
#include "platform/ptr.h"
#include "platform/runnable.h"
#include "gtest/gtest.h"
#include "absl/time/clock.h"

namespace location {
namespace nearby {
namespace {

using SamplePipe = Pipe;

TEST(PipeTest, SimpleWriteRead) {
  auto pipe = MakeRefCountedPtr(new SamplePipe());

  ScopedPtr<Ptr<InputStream>> input_stream(SamplePipe::createInputStream(pipe));
  ScopedPtr<Ptr<OutputStream>> output_stream(
      SamplePipe::createOutputStream(pipe));

  std::string data("ABCD");
  ASSERT_EQ(Exception::NONE, output_stream->write(MakeConstPtr(
                                 new ByteArray(data.data(), data.size()))));

  ExceptionOr<ConstPtr<ByteArray>> read_data = input_stream->read();
  ASSERT_TRUE(read_data.ok());
  ScopedPtr<ConstPtr<ByteArray>> scoped_read_data(read_data.result());
  ASSERT_EQ(data.size(), scoped_read_data->size());
  ASSERT_EQ(0, memcmp(data.data(), scoped_read_data->getData(),
                      scoped_read_data->size()));
}

TEST(PipeTest, WriteEndClosedBeforeRead) {
  auto pipe = MakeRefCountedPtr(new SamplePipe());

  ScopedPtr<Ptr<InputStream>> input_stream(SamplePipe::createInputStream(pipe));
  ScopedPtr<Ptr<OutputStream>> output_stream(
      SamplePipe::createOutputStream(pipe));

  std::string data("ABCD");
  ASSERT_EQ(Exception::NONE, output_stream->write(MakeConstPtr(
                                 new ByteArray(data.data(), data.size()))));

  // Close the write end before the read end has even begun reading.
  ASSERT_EQ(Exception::NONE, output_stream->close());

  // We should still be able to read what was written.
  ExceptionOr<ConstPtr<ByteArray>> read_data = input_stream->read();
  ASSERT_TRUE(read_data.ok());
  ScopedPtr<ConstPtr<ByteArray>> scoped_read_data(read_data.result());
  ASSERT_EQ(data.size(), scoped_read_data->size());
  ASSERT_EQ(0, memcmp(data.data(), scoped_read_data->getData(),
                      scoped_read_data->size()));

  // And after that, we should get our indication that all the data that could
  // ever be read, has already been read.
  read_data = input_stream->read();
  ASSERT_TRUE(read_data.ok());
  ASSERT_TRUE(read_data.result().isNull());
}

TEST(PipeTest, ReadEndClosedBeforeWrite) {
  auto pipe = MakeRefCountedPtr(new SamplePipe());

  ScopedPtr<Ptr<InputStream>> input_stream(SamplePipe::createInputStream(pipe));
  ScopedPtr<Ptr<OutputStream>> output_stream(
      SamplePipe::createOutputStream(pipe));

  // Close the read end before the write end has even begun writing.
  ASSERT_EQ(Exception::NONE, input_stream->close());

  std::string data("ABCD");
  ASSERT_EQ(Exception::IO, output_stream->write(MakeConstPtr(
                               new ByteArray(data.data(), data.size()))));
}

TEST(PipeTest, SizedReadMoreThanFirstChunkSize) {
  auto pipe = MakeRefCountedPtr(new SamplePipe());

  ScopedPtr<Ptr<InputStream>> input_stream(SamplePipe::createInputStream(pipe));
  ScopedPtr<Ptr<OutputStream>> output_stream(
      SamplePipe::createOutputStream(pipe));

  std::string data("ABCD");
  ASSERT_EQ(Exception::NONE, output_stream->write(MakeConstPtr(
                                 new ByteArray(data.data(), data.size()))));

  // Even though we ask for double of what's there in the first chunk, we should
  // get back only what's there in that first chunk, and that's alright.
  ExceptionOr<ConstPtr<ByteArray>> read_data =
      input_stream->read(data.size() * 2);
  ASSERT_TRUE(read_data.ok());
  ScopedPtr<ConstPtr<ByteArray>> scoped_read_data(read_data.result());
  ASSERT_EQ(data.size(), scoped_read_data->size());
  ASSERT_EQ(0, memcmp(data.data(), scoped_read_data->getData(),
                      scoped_read_data->size()));
}

TEST(PipeTest, SizedReadLessThanFirstChunkSize) {
  auto pipe = MakeRefCountedPtr(new SamplePipe());

  ScopedPtr<Ptr<InputStream>> input_stream(SamplePipe::createInputStream(pipe));
  ScopedPtr<Ptr<OutputStream>> output_stream(
      SamplePipe::createOutputStream(pipe));

  // Compose 'data' of 2 parts, to make it easier to validate our expectations.
  std::string data_first_part("ABCD");
  std::string data_second_part("EFGHIJ");
  std::string data = data_first_part + data_second_part;
  ASSERT_EQ(Exception::NONE, output_stream->write(MakeConstPtr(
                                 new ByteArray(data.data(), data.size()))));

  // When we ask for less than what's there in the first chunk, we should get
  // back exactly what we asked for, with the remainder still being available
  // for the next read.
  std::int64_t desired_size = data_first_part.size();
  ExceptionOr<ConstPtr<ByteArray>> first_read_data =
      input_stream->read(desired_size);
  ASSERT_TRUE(first_read_data.ok());
  ScopedPtr<ConstPtr<ByteArray>> scoped_first_read_data(
      first_read_data.result());
  ASSERT_EQ(desired_size, scoped_first_read_data->size());
  ASSERT_EQ(0, memcmp(data_first_part.data(), scoped_first_read_data->getData(),
                      scoped_first_read_data->size()));

  // Now read the remainder, and get everything that ought to have been left.
  std::int64_t remaining_size = data_second_part.size();
  ExceptionOr<ConstPtr<ByteArray>> second_read_data = input_stream->read();
  ASSERT_TRUE(second_read_data.ok());
  ScopedPtr<ConstPtr<ByteArray>> scoped_second_read_data(
      second_read_data.result());
  ASSERT_EQ(remaining_size, scoped_second_read_data->size());
  ASSERT_EQ(0,
            memcmp(data_second_part.data(), scoped_second_read_data->getData(),
                   scoped_second_read_data->size()));
}

TEST(PipeTest, ReadAfterInputStreamClosed) {
  auto pipe = MakeRefCountedPtr(new SamplePipe());

  ScopedPtr<Ptr<InputStream>> input_stream(SamplePipe::createInputStream(pipe));
  ScopedPtr<Ptr<OutputStream>> output_stream(
      SamplePipe::createOutputStream(pipe));

  input_stream->close();

  ExceptionOr<ConstPtr<ByteArray>> read_data = input_stream->read();
  ASSERT_TRUE(!read_data.ok());
  ASSERT_EQ(Exception::IO, read_data.exception());
}

TEST(PipeTest, WriteAfterOutputStreamClosed) {
  auto pipe = MakeRefCountedPtr(new SamplePipe());

  ScopedPtr<Ptr<InputStream>> input_stream(SamplePipe::createInputStream(pipe));
  ScopedPtr<Ptr<OutputStream>> output_stream(
      SamplePipe::createOutputStream(pipe));

  output_stream->close();

  std::string data("ABCD");
  ASSERT_EQ(Exception::IO, output_stream->write(MakeConstPtr(
                               new ByteArray(data.data(), data.size()))));
}

TEST(PipeTest, RepeatedClose) {
  auto pipe = MakeRefCountedPtr(new SamplePipe());

  ScopedPtr<Ptr<InputStream>> input_stream(SamplePipe::createInputStream(pipe));
  ScopedPtr<Ptr<OutputStream>> output_stream(
      SamplePipe::createOutputStream(pipe));

  ASSERT_EQ(Exception::NONE, output_stream->close());
  ASSERT_EQ(Exception::NONE, output_stream->close());
  ASSERT_EQ(Exception::NONE, output_stream->close());

  ASSERT_EQ(Exception::NONE, input_stream->close());
  ASSERT_EQ(Exception::NONE, input_stream->close());
  ASSERT_EQ(Exception::NONE, input_stream->close());
}

class Thread {
 public:
  Thread() : thread_(), attr_(), runnable_() {
    pthread_attr_init(&attr_);
    pthread_attr_setdetachstate(&attr_, PTHREAD_CREATE_JOINABLE);
  }
  ~Thread() { pthread_attr_destroy(&attr_); }

  void start(Ptr<Runnable> runnable) {
    runnable_ = runnable;

    pthread_create(&thread_, &attr_, Thread::body, this);
  }

  void join() {
    pthread_join(thread_, nullptr);

    runnable_.destroy();
  }

 private:
  static void* body(void* args) {
    reinterpret_cast<Thread*>(args)->runnable_->run();
    return nullptr;
  }

  pthread_t thread_;
  pthread_attr_t attr_;
  Ptr<Runnable> runnable_;
};

TEST(PipeTest, ReadBlockedUntilWrite) {
  typedef volatile bool CrossThreadBool;

  class ReaderRunnable : public Runnable {
   public:
    ReaderRunnable(Ptr<InputStream> input_stream,
                   const std::string& expected_read_data,
                   CrossThreadBool* ok_for_read_to_unblock)
        : input_stream_(input_stream),
          expected_read_data_(expected_read_data),
          ok_for_read_to_unblock_(ok_for_read_to_unblock) {}
    ~ReaderRunnable() override {}

    void run() override {
      ExceptionOr<ConstPtr<ByteArray>> read_data = input_stream_->read();

      // Make sure read() doesn't return before it's appropriate.
      if (!*ok_for_read_to_unblock_) {
        FAIL() << "read() unblocked before it was supposed to.";
      }

      // And then run our normal set of checks to make sure the read() was
      // successful.
      ASSERT_TRUE(read_data.ok());
      ScopedPtr<ConstPtr<ByteArray>> scoped_read_data(read_data.result());
      ASSERT_EQ(expected_read_data_.size(), scoped_read_data->size());
      ASSERT_EQ(0,
                memcmp(expected_read_data_.data(), scoped_read_data->getData(),
                       scoped_read_data->size()));
    }

   private:
    ScopedPtr<Ptr<InputStream>> input_stream_;
    const std::string& expected_read_data_;
    CrossThreadBool* ok_for_read_to_unblock_;
  };

  auto pipe = MakeRefCountedPtr(new SamplePipe());

  ScopedPtr<Ptr<OutputStream>> output_stream(
      SamplePipe::createOutputStream(pipe));

  // State shared between this thread (the writer) and reader_thread.
  CrossThreadBool ok_for_read_to_unblock = false;
  std::string data("ABCD");

  // Kick off reader_thread.
  Thread reader_thread;
  reader_thread.start(MakePtr(new ReaderRunnable(
      SamplePipe::createInputStream(pipe), data, &ok_for_read_to_unblock)));

  // Introduce a delay before we actually write anything.
  absl::SleepFor(absl::Seconds(5));
  // Mark that we're done with the delay, and that the write is about to occur
  // (this is slightly earlier than it ought to be, but there's no way to
  // atomically set this from within the implementation of write(), and doing it
  // after is too late for the purposes of this test).
  ok_for_read_to_unblock = true;

  // Perform the actual write.
  ASSERT_EQ(Exception::NONE, output_stream->write(MakeConstPtr(
                                 new ByteArray(data.data(), data.size()))));

  // And wait for reader_thread to finish.
  reader_thread.join();
}

TEST(PipeTest, ConcurrentWriteAndRead) {
  class BaseRunnable : public Runnable {
   protected:
    explicit BaseRunnable(const std::vector<std::string>& chunks)
        : chunks_(chunks), prng_() {}
    ~BaseRunnable() override {}

    void randomSleep() {
      // Generate a random sleep between 100 and 1000 milliseconds.
      absl::SleepFor(absl::Milliseconds(boundedUInt32(100, 1000)));
    }

    const std::vector<std::string>& chunks_;

   private:
    // Both ends of the bounds are inclusive.
    std::uint32_t boundedUInt32(std::uint32_t lower_bound,
                                std::uint32_t upper_bound) {
      return (prng_.nextUInt32() % (upper_bound - lower_bound + 1)) +
             lower_bound;
    }

    Prng prng_;
  };

  class WriterRunnable : public BaseRunnable {
   public:
    WriterRunnable(Ptr<OutputStream> output_stream,
                   const std::vector<std::string>& chunks)
        : BaseRunnable(chunks), output_stream_(output_stream) {}
    ~WriterRunnable() override {}

    void run() override {
      for (std::vector<std::string>::const_iterator it = chunks_.begin();
           it != chunks_.end(); ++it) {
        const std::string& chunk = *it;

        randomSleep();  // Random pauses before each write.
        ASSERT_EQ(Exception::NONE,
                  output_stream_->write(
                      MakeConstPtr(new ByteArray(chunk.data(), chunk.size()))));
      }

      randomSleep();  // A random pause before closing the writer end.
      ASSERT_EQ(Exception::NONE, output_stream_->close());
    }

   private:
    ScopedPtr<Ptr<OutputStream>> output_stream_;
  };

  class ReaderRunnable : public BaseRunnable {
   public:
    ReaderRunnable(Ptr<InputStream> input_stream,
                   const std::vector<std::string>& chunks)
        : BaseRunnable(chunks), input_stream_(input_stream) {}
    ~ReaderRunnable() override {}

    void run() override {
      // First, calculate what we expect to receive, in total.
      std::string expected_data;
      for (std::vector<std::string>::const_iterator it = chunks_.begin();
           it != chunks_.end(); ++it) {
        expected_data += *it;
      }

      // Then, start actually receiving.
      std::string actual_data;
      while (true) {
        randomSleep();  // Random pauses before each read.
        ExceptionOr<ConstPtr<ByteArray>> read_data = input_stream_->read();
        if (read_data.ok()) {
          ScopedPtr<ConstPtr<ByteArray>> scoped_read_data(read_data.result());
          if (scoped_read_data.isNull()) {
            break;  // Normal exit from the read loop.
          }
          actual_data += std::string(scoped_read_data->getData(),
                                     scoped_read_data->size());
        } else {
          break;  // Erroneous exit from the read loop.
        }
      }

      // And once we're done, check that we got everything we expected.
      ASSERT_EQ(expected_data, actual_data);
    }

   private:
    ScopedPtr<Ptr<InputStream>> input_stream_;
  };

  auto pipe = MakeRefCountedPtr(new SamplePipe());

  std::vector<std::string> chunks;
  chunks.push_back("ABCD");
  chunks.push_back("EFGH");
  chunks.push_back("IJKL");

  Thread writer_thread;
  Thread reader_thread;
  writer_thread.start(MakePtr(
      new WriterRunnable(SamplePipe::createOutputStream(pipe), chunks)));
  reader_thread.start(
      MakePtr(new ReaderRunnable(SamplePipe::createInputStream(pipe), chunks)));
  writer_thread.join();
  reader_thread.join();
}

}  // namespace
}  // namespace nearby
}  // namespace location
