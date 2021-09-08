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

#include "platform/impl/windows/thread_pool.h"

#include <stdio.h>

#include <iomanip>
#include <iostream>

#include "platform/impl/windows/runner.h"
#include "platform/public/logging.h"

namespace location {
namespace nearby {
namespace windows {

#define POOL_NAME_BUFFER_SIZE 64
#define EVENT_NAME_BUFFER_SIZE 64

__declspec(align(8)) volatile long ThreadPool::instance_ =  //  NOLINT
    0;  // NOLINT  because the Windows function takes a volatile long

DWORD WINAPI ThreadPool::_ThreadProc(LPVOID pParam) {
  DWORD wait;
  ThreadPool* pool;
  DWORD threadId = GetCurrentThreadId();
  HANDLE waits[2];
  std::unique_ptr<Runner> runner;

  _ASSERT(pParam != NULL);
  if (NULL == pParam) {
    return -1;
  }

  NEARBY_LOGS(VERBOSE) << "Info: " << __func__
                       << ": Starting thread id: " << threadId;

  pool = static_cast<ThreadPool*>(pParam);
  waits[0] = pool->GetWaitHandle(threadId);
  waits[1] = pool->GetShutdownHandle();

loop_here:
  wait = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
  if (wait == 1) {
    if (pool->CheckThreadStop()) {
      if (pool->GetWorkingThreadCount() < 1) {
        NEARBY_LOGS(VERBOSE) << "Info: " << __func__
                             << ": Pool is being destroyed, and working thread "
                                "count is 0, thread exiting.";

        return 0;
      }
    }
  }

  // a new function was added, go and get it
  runner = nullptr;

  NEARBY_LOGS(VERBOSE) << "Info: " << __func__ << ": On thread id: " << threadId
                       << ", checking for work.";

  if (pool->GetThreadProc(threadId, std::move(runner))) {
    pool->BusyNotify(threadId);
    runner->Run();
    pool->FinishNotify(threadId);  // tell the pool, i am now free
  }

  goto loop_here;

  NEARBY_LOGS(VERBOSE) << "Info: " << __func__
                       << ": Thread shutdown occurred.";

  return 0;
}

ThreadPool::ThreadPool(int nPoolSize, bool bCreateNow)
    : function_list_(std::make_unique<FunctionList>()),
      thread_map_(std::make_unique<ThreadMap>()),
      thread_handles_(nullptr),
      wait_for_threads_to_die_ms_(500),
      notify_shutdown_(nullptr) {
  pool_state_ = State::Destroyed;
  pool_size_ = nPoolSize;

  InitializeCriticalSection(&critical_section_);

  if (bCreateNow) {
    if (!Create()) {
      throw ThreadPoolException("ThreadPool creation failed");
    }
  }
}

bool ThreadPool::Create() {
  if (pool_state_ != State::Destroyed) {
    // To create a new pool, destory the existing one first
    return false;
  }

  char buffer[POOL_NAME_BUFFER_SIZE];
  snprintf(buffer, POOL_NAME_BUFFER_SIZE, "Pool%d",
           (uint32_t)(InterlockedIncrement(
               &ThreadPool::instance_)));  // InterlockedIncrement done here
                                           // since there's no access to the
                                           // instance_ var except through the
                                           // interlocked functions

  pool_name_ = std::string(buffer);

  HANDLE thread;
  DWORD threadId;
  ThreadData threadData;
  std::string eventName;

  // create the event which will signal the threads to stop
  notify_shutdown_ = CreateEvent(NULL, TRUE, FALSE, NULL);
  _ASSERT(notify_shutdown_ != NULL);
  if (!notify_shutdown_) {
    NEARBY_LOGS(ERROR) << "Error: " << __func__
                       << ": Failed to create thread shut down event.";

    return false;
  }

  thread_handles_ = new HANDLE[pool_size_];

  // create the threads
  for (int nIndex = 0; nIndex < pool_size_; nIndex++) {
    char buffer[EVENT_NAME_BUFFER_SIZE];

    snprintf(buffer, EVENT_NAME_BUFFER_SIZE, "PID:%ld IID:%d TDX:%d",
             GetCurrentProcessId(),
             (uint32_t)(InterlockedAdd(&ThreadPool::instance_, 0)), nIndex);

    thread = CreateThread(NULL, 0, ThreadPool::_ThreadProc, this,
                          CREATE_SUSPENDED, &threadId);

    _ASSERT(NULL != thread);

    if (NULL == thread) {
      NEARBY_LOGS(ERROR) << "Error: " << __func__
                         << ": Failed to create thread.";
      return false;
    }

    if (thread) {
//      EnterCriticalSection(&critical_section_);
      // add the entry to the map of threads
      threadData.free = true;
      threadData.wait_handle =
          CreateEventA(NULL, TRUE, FALSE, eventName.c_str());

      threadData.thread_handle = thread;
      threadData.thread_id = threadId;

      EnterCriticalSection(&critical_section_);

      thread_map_->insert(ThreadMap::value_type(threadId, threadData));

      LeaveCriticalSection(&critical_section_);

      ResumeThread(thread);

      NEARBY_LOGS(VERBOSE) << "Info: " << __func__
                           << ": Thread created, handle: " << thread
                           << ", id: " << threadId;
    } else {
      NEARBY_LOGS(ERROR) << "Error: " << __func__
                         << ": Failed to create thread.";
      return false;
    }
  }

  pool_state_ = State::Ready;
  return true;
}

ThreadPool::~ThreadPool() {
  Destroy();
  ReleaseMemory();
  DeleteCriticalSection(&critical_section_);
}

void ThreadPool::ReleaseMemory() {
  // empty all collections
  EnterCriticalSection(&critical_section_);

  NEARBY_LOGS(VERBOSE) << "Info: " << __func__
                       << ": Clearing the function list.";

  function_list_->clear();

  NEARBY_LOGS(VERBOSE) << "Info: " << __func__
                       << ": Clearing the thread map.";

  thread_map_->clear();

  LeaveCriticalSection(&critical_section_);
}

void ThreadPool::Destroy() {
  if (pool_state_ == State::Destroying || pool_state_ == State::Destroyed)
    return;

  NEARBY_LOGS(VERBOSE) << "Info: " << __func__ << ": Destroying thread pool.";

  pool_state_ = State::Destroying;

  EnterCriticalSection(&critical_section_);

  ThreadMap::iterator iter = thread_map_->begin();
  int index = 0;

  // Build an array of handles
  while (iter != thread_map_->end()) {
    thread_handles_[index] = iter->second.thread_handle;
    index++;
    iter++;
  }

  LeaveCriticalSection(&critical_section_);

  // tell all threads to shutdown.
  _ASSERT(NULL != notify_shutdown_);
  SetEvent(GetShutdownHandle());

  NEARBY_LOGS(VERBOSE) << "Info: " << __func__
                       << ": Setting waits for the threads to exit.";

  if (pool_size_ == 1) {
    auto wait = WaitForSingleObject(thread_handles_[0], INFINITE);
  } else {
    auto wait = WaitForMultipleObjects(index, thread_handles_, true, INFINITE);
  }

  NEARBY_LOGS(VERBOSE) << "Info: " << __func__
                       << ": All threads have exited.";

  delete[] thread_handles_;

  // close the shutdown event
  CloseHandle(notify_shutdown_);
  notify_shutdown_ = NULL;

  EnterCriticalSection(&critical_section_);

  ThreadMap::iterator threadMapIterator;

  // walk through the events and threads and close them all
  for (threadMapIterator = thread_map_->begin();
       threadMapIterator != thread_map_->end(); threadMapIterator++) {
    NEARBY_LOGS(VERBOSE) << "Info: " << __func__ << ": Closing thread handle: "
                         << threadMapIterator->second.thread_handle
                         << " thread id: "
                         << threadMapIterator->second.thread_id
                         << " wait_handle: "
                         << threadMapIterator->second.wait_handle;

    CloseHandle(threadMapIterator->second.wait_handle);
    CloseHandle(threadMapIterator->second.thread_handle);
  }

  LeaveCriticalSection(&critical_section_);

  NEARBY_LOGS(VERBOSE) << "Info: " << __func__
                       << ": Sending the shutdown event.";

  ReleaseMemory();  // free any remaining UserPoolData objects

  InterlockedDecrement(&ThreadPool::instance_);

  pool_state_ = State::Destroyed;
}

int ThreadPool::GetPoolSize() { return pool_size_; }

void ThreadPool::SetPoolSize(int nSize) {
  _ASSERT(nSize > 0);

  if (nSize <= 0) {
    return;
  }

  pool_size_ = nSize;
}

HANDLE ThreadPool::GetShutdownHandle() { return notify_shutdown_; }

bool ThreadPool::GetThreadProc(DWORD threadId,
                               std::unique_ptr<Runner>&& runner) {
  // get the first function info in the function list
  FunctionList::iterator functionListIterator;
  bool haveAnotherRunner = false;

  EnterCriticalSection(&critical_section_);

  functionListIterator = function_list_->begin();

  if (functionListIterator != function_list_->end()) {
    runner = std::move(*functionListIterator);

    NEARBY_LOGS(VERBOSE) << "Info: " << __func__
                         << ": popping runner from the front.";

    function_list_->pop_front();  // remove the function from the list

    haveAnotherRunner = true;
  } else {
    thread_map_->at(threadId).free = true;
    ResetEvent(thread_map_->at(threadId).wait_handle);
  }

  LeaveCriticalSection(&critical_section_);

  return haveAnotherRunner;
}

void ThreadPool::FinishNotify(DWORD threadId) {
  ThreadMap::iterator threadMapIterator;

  EnterCriticalSection(&critical_section_);

  threadMapIterator = thread_map_->find(threadId);

  if (threadMapIterator == thread_map_->end())  // if search found no elements
  {
    _ASSERT(!"No matching thread found.");
  } else {
    thread_map_->at(threadId).free = true;

    if (!function_list_->empty()) {
      // there are some more functions that need servicing, lets do that.
      // By not doing anything here we are letting the thread go back and
      // check the function list and pick up a function and execute it.
      thread_map_->at(threadId).free = false;
    } else {
      ResetEvent(thread_map_->at(threadId).wait_handle);
    }
  }

  LeaveCriticalSection(&critical_section_);
}

void ThreadPool::BusyNotify(DWORD threadId) {
  ThreadMap::iterator iter;

  EnterCriticalSection(&critical_section_);

  iter = thread_map_->find(threadId);

  if (iter == thread_map_->end())  // if search found no elements
  {
    _ASSERT(!"No matching thread found.");
  } else {
    thread_map_->at(threadId).free = false;
  }

  LeaveCriticalSection(&critical_section_);
}

bool ThreadPool::Run(std::unique_ptr<Runner> runner) {
  if (pool_state_ == State::Destroying || pool_state_ == State::Destroyed)
    return false;

  _ASSERT(runner != NULL);

  AddRunner(std::move(runner));

  // See if any threads are free
  ThreadMap::iterator iterator;
  ThreadData ThreadData;

  EnterCriticalSection(&critical_section_);

  for (iterator = thread_map_->begin(); iterator != thread_map_->end();
       iterator++) {
    ThreadData = (*iterator).second;

    if (ThreadData.free) {
      // here is a free thread, put it to work
      iterator->second.free = false;
      SetEvent(ThreadData.wait_handle);
      // this thread will now call GetThreadProc() and pick up the next
      // function in the list.
      break;
    }
  }

  LeaveCriticalSection(&critical_section_);

  return true;
}

void ThreadPool::AddRunner(std::unique_ptr<Runner> runner) {
  // add it to the list
  runner->thread_pool_ = this;

  NEARBY_LOGS(VERBOSE) << "Info: " << __func__
                       << ": pushing new runner to the back.";

  EnterCriticalSection(&critical_section_);

  function_list_->push_back(std::move(runner));

  LeaveCriticalSection(&critical_section_);
}

HANDLE ThreadPool::GetWaitHandle(DWORD dwThreadId) {
  HANDLE hWait = NULL;
  ThreadMap::iterator iter;

  EnterCriticalSection(&critical_section_);

  iter = thread_map_->find(dwThreadId);

  if (iter != thread_map_->end())  // if search found no elements
  {
    hWait = thread_map_->at(dwThreadId).wait_handle;
  }

  LeaveCriticalSection(&critical_section_);

  return hWait;
}

bool ThreadPool::CheckThreadStop() {
  EnterCriticalSection(&critical_section_);

  bool bRet =
      (pool_state_ == State::Destroying || pool_state_ == State::Destroyed);

  LeaveCriticalSection(&critical_section_);

  return bRet;
}

int ThreadPool::GetWorkingThreadCount() {
  ThreadMap::iterator iter;
  ThreadData threadData;

  int nCount = 0;

  EnterCriticalSection(&critical_section_);

  for (iter = thread_map_->begin(); iter != thread_map_->end(); iter++) {
    threadData = (*iter).second;

    if (function_list_->empty()) {
      threadData.free = true;
    }

    if (!threadData.free) {
      nCount++;
    }
  }

  LeaveCriticalSection(&critical_section_);

  return nCount;
}

State ThreadPool::GetState() { return pool_state_; }

}  // namespace windows
}  // namespace nearby
}  // namespace location
