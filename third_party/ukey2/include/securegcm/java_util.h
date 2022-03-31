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

// Utility functions for Java-compatible operations.
#ifndef SECURITY_CRYPTAUTH_LIB_SECUREGCM_JAVA_UTIL_H_
#define SECURITY_CRYPTAUTH_LIB_SECUREGCM_JAVA_UTIL_H_

#include "securemessage/byte_buffer.h"

namespace securegcm {
namespace java_util {

// Perform multiplication with Java overflow semantics
// (https://docs.oracle.com/javase/specs/jls/se8/html/jls-15.html):
//   If an integer multiplication overflows, then the result is the low-order
//   bits of the mathematical product as represented in some sufficiently
//   large two's-complement format.
int32_t JavaMultiply(int32_t lhs, int32_t rhs);

// Perform addition with Java overflow semantics:
// (https://docs.oracle.com/javase/specs/jls/se8/html/jls-15.html):
//   If an integer addition overflows, then the result is the low-order bits of
//   the mathematical sum as represented in some sufficiently large
//   two's-complement format.
int32_t JavaAdd(int32_t lhs, int32_t rhs);

// To be compatible with the Java implementation, we need to use the same
// algorithm as the Arrays#hashCode(byte[]) function in Java:
//  "The value returned by this method is the same value that would be obtained
//  by invoking the hashCode method on a List containing a sequence of Byte
//  instances representing the elements of a in the same order."
//
// According to List#hashCode(), this algorithm is:
//   int hashCode = 1;
//   for (Byte b : list) {
//     hashCode = 31 * hashCode + (b == null ? b : b.hashCode());
//   }
//
// Finally, Byte#hashCode() is defined as "equal to the result of invoking
// Byte#intValue()".
int32_t JavaHashCode(const securemessage::ByteBuffer& byte_buffer);

}  // namespace java_util
}  // namespace securegcm

#endif  // SECURITY_CRYPTAUTH_LIB_SECUREGCM_JAVA_UTIL_H_
