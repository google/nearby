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
#ifndef PLATFORM_IMPL_WINDOWS_THREAD_POOL_H_
#define PLATFORM_IMPL_WINDOWS_THREAD_POOL_H_
#include <windows.h>

#include <functional>
#include <list>
#include <map>
#include <stdexcept>

#include "internal/platform/implementation/windows/runner.h"
namespace location {
namespace nearby {
namespace windows {

// This is the number of threads that will be started initially if the pool
// size is greater than 4, or if the pool size is greater than the number
// of cores present, including virtual cores
#define STARTUP_THREAD_COUNT 4

class ThreadPoolException : public std::runtime_error {
 public:
  ThreadPoolException() : std::runtime_error("") {}
  ThreadPoolException(const std::string& message)
      : std::runtime_error(message), message_(message) {}
  virtual const char* what() const throw() {
    return message_.c_str();
  }

 private:
  const std::string message_;
};

// all functions passed in by clients will be initially stored in this list.
typedef std::list<std::unique_ptr<Runner>> FunctionList;
// info about threads in the pool will be saved using this struct.
typedef struct tagThreadData {
  bool free;
  HANDLE wait_handle;
  HANDLE thread_handle;
  DWORD thread_id;
} ThreadData;
// info about all threads belonging to this pool will be stored in this map
typedef std::map<DWORD, std::unique_ptr<ThreadData>>
    ThreadMap;
enum class State {
  Ready,       // has been created
  Destroying,  // in the process of getting destroyed, no request is processed /
               // accepted
  Destroyed  // Destroyed, no threads are available, request can still be queued
};
class ThreadPool {
 public:
  ThreadPool(int nPoolSize, bool bCreateNow);
  virtual ~ThreadPool();
  bool Create();   // creates the thread pool
  void Destroy();  // destroy the thread pool
  int GetPoolSize();
  void SetPoolSize(int);
  bool Run(std::unique_ptr<Runner> runObject);
  bool CheckThreadStop();
  int GetWorkingThreadCount();
  State GetState();

 private:
  static DWORD WINAPI _ThreadProc(LPVOID);
  std::unique_ptr<FunctionList> function_list_;
  std::unique_ptr<ThreadMap> thread_map_;
  HANDLE* thread_handles_ = nullptr;
  int pool_size_;
  int wait_for_threads_to_die_ms_;  // In milli-seconds
  std::string pool_name_;           // To assist in logging and debug
  HANDLE notify_shutdown_;          // notifies threads that a new function
                                    // is added
  volatile State pool_state_;
  static __declspec(
      align(8)) volatile long instance_;  //  NOLINT Windows function takes
                                          //  volatile long
  CRITICAL_SECTION critical_section_;

  bool GetThreadProc(DWORD dwThreadId, std::unique_ptr<Runner>&& runner);
  void FinishNotify(DWORD dwThreadId);
  void BusyNotify(DWORD dwThreadId);
  void ReleaseMemory();
  HANDLE GetWaitHandle(DWORD dwThreadId);
  HANDLE GetShutdownHandle();
  void AddRunner(std::unique_ptr<Runner> runner);
  DWORD CreateThreadPoolThread(HANDLE* handles);
};

}  // namespace windows
}  // namespace nearby
}  // namespace location
#endif  //  PLATFORM_IMPL_WINDOWS_THREAD_POOL_H_
