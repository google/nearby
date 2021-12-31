// Copyright 2021 Google LLC
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

#ifndef PLATFORM_IMPL_WINDOWS_TEST_DATA_H_
#define PLATFORM_IMPL_WINDOWS_TEST_DATA_H_

#define INVALID_ARGUMENT_TEXT "max_concurrency"
#define THREADPOOL_MAX_SIZE_TEXT "Thread pool max size exceeded."
#define RUNNABLE_TEXT std::string("runnable ")
#define RUNNABLE_0_TEXT RUNNABLE_TEXT + std::string("0")
#define RUNNABLE_1_TEXT RUNNABLE_TEXT + std::string("1")
#define RUNNABLE_2_TEXT RUNNABLE_TEXT + std::string("2")
#define RUNNABLE_3_TEXT RUNNABLE_TEXT + std::string("3")
#define RUNNABLE_4_TEXT RUNNABLE_TEXT + std::string("4")
#define RUNNABLE_SEPARATOR_TEXT std::string(", ")
#define RUNNABLE_ALL_TEXT                                                \
  (RUNNABLE_0_TEXT + RUNNABLE_SEPARATOR_TEXT + RUNNABLE_1_TEXT +         \
   RUNNABLE_SEPARATOR_TEXT + RUNNABLE_2_TEXT + RUNNABLE_SEPARATOR_TEXT + \
   RUNNABLE_3_TEXT + RUNNABLE_SEPARATOR_TEXT + RUNNABLE_4_TEXT +         \
   RUNNABLE_SEPARATOR_TEXT)

#endif  // PLATFORM_IMPL_WINDOWS_TEST_DATA_H_

