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

#ifndef PLATFORM_RUNNABLE_H_
#define PLATFORM_RUNNABLE_H_

namespace location {
namespace nearby {

// The Runnable interface should be implemented by any class whose instances are
// intended to be executed by a thread. The class must define a method named
// run() with no arguments.
//
// https://docs.oracle.com/javase/8/docs/api/java/lang/Runnable.html
class Runnable {
 public:
  virtual ~Runnable() {}

  virtual void run() = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_RUNNABLE_H_
