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

#include "internal/platform/pipe.h"

#include <pthread.h>

#include <atomic>
#include <cstring>
#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "internal/platform/prng.h"
#include "internal/platform/runnable.h"

namespace nearby {

TEST(PipeTest, ConstructorDestructorWorks) {
  Pipe pipe;
  SUCCEED();
}

TEST(PipeTest, SimpleWriteRead) {
  Pipe pipe;
  InputStream& input_stream{pipe.GetInputStream()};
  OutputStream& output_stream{pipe.GetOutputStream()};

  std::string data("ABCD");
  EXPECT_TRUE(output_stream.Write(ByteArray(data)).Ok());

  ExceptionOr<ByteArray> read_data = input_stream.Read(Pipe::kChunkSize);
  EXPECT_TRUE(read_data.ok());
  EXPECT_EQ(data, std::string(read_data.result()));
}

TEST(PipeTest, WriteEndClosedBeforeRead) {
  Pipe pipe;
  InputStream& input_stream{pipe.GetInputStream()};
  OutputStream& output_stream{pipe.GetOutputStream()};

  std::string data("ABCD");
  EXPECT_TRUE(output_stream.Write(ByteArray(data)).Ok());

  // Close the write end before the read end has even begun reading.
  EXPECT_TRUE(output_stream.Close().Ok());

  // We should still be able to read what was written.
  ExceptionOr<ByteArray> read_data = input_stream.Read(Pipe::kChunkSize);
  EXPECT_TRUE(read_data.ok());
  EXPECT_EQ(data, std::string(read_data.result()));

  // And after that, we should get our indication that all the data that could
  // ever be read, has already been read.
  read_data = input_stream.Read(Pipe::kChunkSize);
  EXPECT_TRUE(read_data.ok());
  EXPECT_TRUE(read_data.result().Empty());
}

TEST(PipeTest, ReadEndClosedBeforeWrite) {
  Pipe pipe;
  InputStream& input_stream{pipe.GetInputStream()};
  OutputStream& output_stream{pipe.GetOutputStream()};

  // Close the read end before the write end has even begun writing.
  EXPECT_TRUE(input_stream.Close().Ok());

  std::string data("ABCD");
  EXPECT_TRUE(output_stream.Write(ByteArray(data)).Raised(Exception::kIo));
}

TEST(PipeTest, SizedReadMoreThanFirstChunkSize) {
  Pipe pipe;
  InputStream& input_stream{pipe.GetInputStream()};
  OutputStream& output_stream{pipe.GetOutputStream()};

  std::string data("ABCD");
  EXPECT_TRUE(output_stream.Write(ByteArray(data)).Ok());

  // Even though we ask for double of what's there in the first chunk, we should
  // get back only what's there in that first chunk, and that's alright.
  ExceptionOr<ByteArray> read_data = input_stream.Read(data.size() * 2);
  EXPECT_TRUE(read_data.ok());
  EXPECT_EQ(data, std::string(read_data.result()));
}

TEST(PipeTest, SizedReadLessThanFirstChunkSize) {
  Pipe pipe;
  InputStream& input_stream{pipe.GetInputStream()};
  OutputStream& output_stream{pipe.GetOutputStream()};

  std::string data_first_part("ABCD");
  std::string data_second_part("EFGHIJ");
  std::string data = data_first_part + data_second_part;
  EXPECT_TRUE(output_stream.Write(ByteArray(data)).Ok());

  // When we ask for less than what's there in the first chunk, we should get
  // back exactly what we asked for, with the remainder still being available
  // for the next read.
  std::int64_t desired_size = data_first_part.size();
  ExceptionOr<ByteArray> first_read_data = input_stream.Read(desired_size);
  EXPECT_TRUE(first_read_data.ok());
  EXPECT_EQ(data_first_part, std::string(first_read_data.result()));

  // Now read the remainder, and get everything that ought to have been left.
  ExceptionOr<ByteArray> second_read_data = input_stream.Read(Pipe::kChunkSize);
  EXPECT_TRUE(second_read_data.ok());
  EXPECT_EQ(data_second_part, std::string(second_read_data.result()));
}

TEST(PipeTest, ReadAfterInputStreamClosed) {
  Pipe pipe;
  InputStream& input_stream{pipe.GetInputStream()};

  input_stream.Close();

  ExceptionOr<ByteArray> read_data = input_stream.Read(Pipe::kChunkSize);
  EXPECT_TRUE(!read_data.ok());
  EXPECT_TRUE(read_data.GetException().Raised(Exception::kIo));
}

TEST(PipeTest, WriteAfterOutputStreamClosed) {
  Pipe pipe;
  OutputStream& output_stream{pipe.GetOutputStream()};

  output_stream.Close();

  std::string data("ABCD");
  EXPECT_TRUE(output_stream.Write(ByteArray(data)).Raised(Exception::kIo));
}

TEST(PipeTest, RepeatedClose) {
  Pipe pipe;
  InputStream& input_stream{pipe.GetInputStream()};
  OutputStream& output_stream{pipe.GetOutputStream()};

  EXPECT_TRUE(output_stream.Close().Ok());
  EXPECT_TRUE(output_stream.Close().Ok());
  EXPECT_TRUE(output_stream.Close().Ok());

  EXPECT_TRUE(input_stream.Close().Ok());
  EXPECT_TRUE(input_stream.Close().Ok());
  EXPECT_TRUE(input_stream.Close().Ok());
}

class Thread {
 public:
  Thread() : thread_(), attr_(), runnable_() {
    pthread_attr_init(&attr_);
    pthread_attr_setdetachstate(&attr_, PTHREAD_CREATE_JOINABLE);
  }
  ~Thread() { pthread_attr_destroy(&attr_); }

  void Start(Runnable runnable) {
    runnable_ = std::move(runnable);

    pthread_create(&thread_, &attr_, Thread::Body, this);
  }

  void Join() { pthread_join(thread_, nullptr); }

 private:
  static void* Body(void* args) {
    reinterpret_cast<Thread*>(args)->runnable_();
    return nullptr;
  }

  pthread_t thread_;
  pthread_attr_t attr_;
  Runnable runnable_;
};

TEST(PipeTest, ReadBlockedUntilWrite) {
  using CrossThreadBool = std::atomic_bool;

  class ReaderRunnable {
   public:
    ReaderRunnable(InputStream* input_stream,
                   absl::string_view expected_read_data,
                   CrossThreadBool* ok_for_read_to_unblock)
        : input_stream_(input_stream),
          expected_read_data_(expected_read_data),
          ok_for_read_to_unblock_(ok_for_read_to_unblock) {}
    ~ReaderRunnable() = default;

    // Signature "void()" satisfies Runnable.
    void operator()() {
      ExceptionOr<ByteArray> read_data = input_stream_->Read(Pipe::kChunkSize);

      // Make sure read() doesn't return before it's appropriate.
      if (!*ok_for_read_to_unblock_) {
        FAIL() << "read() unblocked before it was supposed to.";
      }

      // And then run our normal set of checks to make sure the read() was
      // successful.
      EXPECT_TRUE(read_data.ok());
      EXPECT_EQ(expected_read_data_, std::string(read_data.result()));
    }

   private:
    InputStream* input_stream_;
    const std::string expected_read_data_;
    CrossThreadBool* ok_for_read_to_unblock_;
  };

  Pipe pipe;
  OutputStream& output_stream{pipe.GetOutputStream()};

  // State shared between this thread (the writer) and reader_thread.
  CrossThreadBool ok_for_read_to_unblock = false;
  std::string data("ABCD");

  // Kick off reader_thread.
  Thread reader_thread;
  reader_thread.Start(
      ReaderRunnable(&pipe.GetInputStream(), data, &ok_for_read_to_unblock));

  // Introduce a delay before we actually write anything.
  absl::SleepFor(absl::Seconds(5));
  // Mark that we're done with the delay, and that the write is about to occur
  // (this is slightly earlier than it ought to be, but there's no way to
  // atomically set this from within the implementation of write(), and doing it
  // after is too late for the purposes of this test).
  ok_for_read_to_unblock = true;

  // Perform the actual write.
  EXPECT_TRUE(output_stream.Write(ByteArray(data)).Ok());

  // And wait for reader_thread to finish.
  reader_thread.Join();
}

TEST(PipeTest, ConcurrentWriteAndRead) {
  class BaseRunnable {
   protected:
    explicit BaseRunnable(const std::vector<std::string>& chunks)
        : chunks_(chunks) {}
    virtual ~BaseRunnable() = default;

    void RandomSleep() {
      // Generate a random sleep between 100 and 1000 milliseconds.
      absl::SleepFor(absl::Milliseconds(BoundedUint32(100, 1000)));
    }

    const std::vector<std::string>& chunks_;

   private:
    // Both ends of the bounds are inclusive.
    std::uint32_t BoundedUint32(std::uint32_t lower_bound,
                                std::uint32_t upper_bound) {
      return (Prng().NextUint32() % (upper_bound - lower_bound + 1)) +
             lower_bound;
    }
  };

  class WriterRunnable : public BaseRunnable {
   public:
    WriterRunnable(OutputStream* output_stream,
                   const std::vector<std::string>& chunks)
        : BaseRunnable(chunks), output_stream_(output_stream) {}
    ~WriterRunnable() override = default;

    void operator()() {
      for (auto& chunk : chunks_) {
        RandomSleep();  // Random pauses before each write.
        EXPECT_TRUE(output_stream_->Write(ByteArray(chunk)).Ok());
      }

      RandomSleep();  // A random pause before closing the writer end.
      EXPECT_TRUE(output_stream_->Close().Ok());
    }

   private:
    OutputStream* output_stream_;
  };

  class ReaderRunnable : public BaseRunnable {
   public:
    ReaderRunnable(InputStream* input_stream,
                   const std::vector<std::string>& chunks)
        : BaseRunnable(chunks), input_stream_(input_stream) {}
    ~ReaderRunnable() override = default;

    void operator()() {
      // First, calculate what we expect to receive, in total.
      std::string expected_data;
      for (auto& chunk : chunks_) {
        expected_data += chunk;
      }

      // Then, start actually receiving.
      std::string actual_data;
      while (true) {
        RandomSleep();  // Random pauses before each read.
        ExceptionOr<ByteArray> read_data =
            input_stream_->Read(Pipe::kChunkSize);
        if (read_data.ok()) {
          ByteArray result = read_data.result();
          if (result.Empty()) {
            break;  // Normal exit from the read loop.
          }
          actual_data += std::string(result);
        } else {
          break;  // Erroneous exit from the read loop.
        }
      }

      // And once we're done, check that we got everything we expected.
      EXPECT_EQ(expected_data, actual_data);
    }

   private:
    InputStream* input_stream_;
  };

  Pipe pipe;

  std::vector<std::string> chunks;
  chunks.push_back("ABCD");
  chunks.push_back("EFGH");
  chunks.push_back("IJKL");

  Thread writer_thread;
  Thread reader_thread;
  writer_thread.Start(WriterRunnable(&pipe.GetOutputStream(), chunks));
  reader_thread.Start(ReaderRunnable(&pipe.GetInputStream(), chunks));
  writer_thread.Join();
  reader_thread.Join();
}

}  // namespace nearby
