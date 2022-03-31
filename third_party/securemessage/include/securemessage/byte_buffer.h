/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//
// A utility class for helping to interact with contiguous memory regions in a
// way that's both c++ friendly and works with OpenSSL's legacy C requirements.
//
// Will securely destroy enclosed data when deallocated
//

#ifndef SECUREMESSAGE_BYTE_BUFFER_H_
#define SECUREMESSAGE_BYTE_BUFFER_H_

#include <stddef.h>
#include <cstdint>
#include <memory>
#include <vector>

#include "securemessage/common.h"

namespace securemessage {

class ByteBuffer {
 public:
  // Create an empty byte buffer
  ByteBuffer();

  // Create a buffer of a certain size. Buffer will be filled with zeros.
  explicit ByteBuffer(size_t size);

  // Create a buffer initialized with bytes from a string
  explicit ByteBuffer(const string& source_data);

  // Create a buffer initialized with data from a const char* and a length
  explicit ByteBuffer(const char* source_data, size_t length);

  // Create a buffer initialized with data from a const char* (assumes null
  // termination)
  explicit ByteBuffer(const char* source_data);

  // Create a buffer initialized with data from a uint8_t
  explicit ByteBuffer(const uint8_t* source_data, size_t length);

  // Fill with the specified data.  Any old data is discarded
  void SetData(const uint8_t* source_data, size_t length);

  // Appends num_bytes of value byte to the end of the existing data
  // Currently may violate secure memory guarantees by leaking some data on the
  // heap
  void Append(size_t num_bytes, const uint8_t byte);

  // Prepends a string to the beginning of this ByteBuffer
  // Currently may violate secure memory guarantees by leaking some data on the
  // heap
  void Prepend(const string& str);

  // Concatenates two byte buffers.  Placing a before b.  If a is empty, then
  // the result will be b.  If b is empty, the result will be a.  If both are
  // empty, the result will be an empty byte buffer.
  static ByteBuffer Concat(const ByteBuffer& a, const ByteBuffer& b);

  // Get size of internal buffer
  size_t size() const;

  // Returns a unique pointer to new ByteBuffer of a given length starting from
  // a given offset.
  // Returns a unique pointer to an empty ByteBuffer if length is 0.
  // Returns a nullptr if the specified parameters are invalid.
  // Note that valid offset begins at 0
  std::unique_ptr<ByteBuffer> SubArray(size_t offset, size_t length) const;

  // Get mutable memory as unsigned char*
  unsigned char* MutableUChar();

  // Get immutable memory as unsigned char*
  unsigned const char* ImmutableUChar() const;

  // Get mutable memory as uint8_t*
  uint8_t* MutableUInt8();

  // Get immutable memory as uint8_t*
  const uint8_t* ImmutableUInt8() const;

  // Get a copy of the memory in a C++ string
  string String() const;

  // Get a copy of the memory in a C++ vector
  std::vector<uint8_t> Vector() const;

  // Returns true if the bytes in the other buffer are the same as in this
  // buffer, false otherwise. Note: This function does NOT perform an early exit
  // when it encounters a mismatching element in order to guard against timing
  // attacks.
  bool Equals(const ByteBuffer& other) const;

  // Returns true if the bytes in the other string are the same as in this
  // buffer, false otherwise. Note: This function does NOT perform an early exit
  // when it encounters a mismatching element in order to guard against timing
  // attacks.
  bool Equals(const string& other) const;

  // Returns a copy of the data as a printable hex string -- for easy debugging
  string AsDebugHexString() const;

  // Securely resets the ByteBuffer by securely deleting any stored data.
  void Clear();

  // Securely destroys the ByteBuffer
  ~ByteBuffer();

 private:
  // We rely on the fact that vectors are guaranteed to use contiguous memory
  // for storing elements.
  // TODO(aczeskis): define a custom version of vector that has a wiping
  // deallocator.
  std::vector<uint8_t> data_;

  // Overwrites all elements in data vector one time.  Does not resize data
  // vector.
  void Wipe();

  // Helper function to wipe a specified buffer.
  void WipeBuffer(uint8_t* buffer, size_t length);
};

}  // namespace securemessage
#endif  // SECUREMESSAGE_BYTE_BUFFER_H_
