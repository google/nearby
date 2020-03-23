#ifndef PLATFORM_PIPE_H_
#define PLATFORM_PIPE_H_

#include <cstdint>
#include <deque>

#include "platform/api/condition_variable.h"
#include "platform/api/input_stream.h"
#include "platform/api/lock.h"
#include "platform/api/output_stream.h"
#include "platform/byte_array.h"
#include "platform/exception.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

namespace pipe {

template <typename>
class PipeInputStream;
template <typename>
class PipeOutputStream;

}  // namespace pipe

template <typename Platform>
class Pipe {
 public:
  Pipe();
  ~Pipe();

  // The returned InputStream is auto-destroyed when no longer referenced.
  static Ptr<InputStream> createInputStream(Ptr<Pipe>);
  // The returned OutputStream is auto-destroyed when no longer referenced.
  static Ptr<OutputStream> createOutputStream(Ptr<Pipe>);

 private:
  //////////////////////////////////////////////////////////////////////////////
  // Everything in this first private: section is only used by PipeInputStream
  // and PipeOutputStream, thus forming the interface presented to those 2
  // classes.
  //////////////////////////////////////////////////////////////////////////////

  template <typename>
  friend class pipe::PipeInputStream;
  template <typename>
  friend class pipe::PipeOutputStream;

  ExceptionOr<ConstPtr<ByteArray> > read(std::int64_t size);
  Exception::Value write(ConstPtr<ByteArray> data);

  void markInputStreamClosed();
  void markOutputStreamClosed();

 private:
  Exception::Value writeLocked(ConstPtr<ByteArray> data);

  bool eitherStreamClosed() const;

  ScopedPtr<Ptr<Lock> > lock_;
  ScopedPtr<Ptr<ConditionVariable> > cond_;
  typedef std::deque<ConstPtr<ByteArray> > BufferType;
  BufferType buffer_;
  bool input_stream_closed_;
  bool output_stream_closed_;
  bool read_all_chunks_;
};

}  // namespace nearby
}  // namespace location

#include "platform/pipe.cc"

#endif  // PLATFORM_PIPE_H_
