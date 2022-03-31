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

#ifndef SECUREMESSAGE_UTIL_H_
#define SECUREMESSAGE_UTIL_H_

#include <memory>
#include <string>

#include "securemessage/common.h"

namespace securemessage {

class Util {
 public:
  //
  // Makes a new std::unique_ptr<string> by taking input data and a length
  //
  static std::unique_ptr<std::string> MakeUniquePtrString(const void* data,
                                                          size_t length);

  //
  // Providing a level of indirection when logging error message.  The idea
  // being that this can be swapped out in platforms that log error messages in
  // some more useful way.
  //
  static void LogError(const std::string& error_message);

  //
  // Provides a level of indirection for managing really bad errors.  A call of
  // this function indicates that an unrecoverable error happened -- perhaps
  // because of an attack or a bug in an underlying crypto provider.  The
  // invocation of this function indicates that any subsequent behavior will be
  // undefined.  This level of indirection is meant to let users of this library
  // figure out how they want to deal with such bad errors.
  //
  // This function should never return.
  //
  static void LogErrorAndAbort [[noreturn]] (const std::string& error_message);

 private:
  virtual ~Util();
};

}  // namespace securemessage

#endif  // SECUREMESSAGE_UTIL_H_
