// Copyright 2022 Google LLC
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
//
// Defines an interface that allows a resource owner to share a resource with
// tasks running on other thread, and protects against use-after-free errors.
// When a resource is borrowed, the borrower has exclusive access to the
// resource. The framework guarantees that the resource stays alive as long it
// is borrowed. Typical usage:
//
// In resource owner:
// Lender<DataType> lender(DataType{});
// Give clients instances of `lender.GetBorrowable()`
//
// In clients:
// Borrowed<DataType> borrowed = borrowable.Borrow();
// if (borrowed) {
//   borrowed->SomeMethod();
// }
//
// When `Lender` goes out of scope, all borrowable instances become invalid and
// attempts to `Borrow()` will fail safely.

#ifndef THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_BORROWABLE_H_
#define THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_BORROWABLE_H_

#include <memory>
#include <utility>

#ifdef NEARBY_CHROMIUM
#include "base/check.h"
#elif defined(NEARBY_SWIFTPM)
#include "internal/platform/logging.h"
#else
#include "absl/log/check.h"  // nogncheck
#endif

#include "internal/platform/mutex.h"
#include "internal/platform/mutex_lock.h"

namespace nearby {

template <typename T>
struct BorrowableSharedData {
  explicit BorrowableSharedData(T resource) : resource(std::move(resource)) {}

  Mutex mutex;
  T resource ABSL_GUARDED_BY(mutex);
  bool released ABSL_GUARDED_BY(mutex) = false;
};

// RAII style accessor to a borrowed object. While the caller holds an instance
// of `Borrowed`, they have exclusive access to the borrowed object. Borrowing
// can fail if the object that we are trying to borrow is already destroyed. The
// caller should check if `Borrowed` is valid before accessing.
template <typename T>
class Borrowed {
 public:
  Borrowed() = default;
  explicit Borrowed(std::shared_ptr<BorrowableSharedData<T>> data)
      : data_(data), lock_(std::make_unique<MutexLock>(&data->mutex)) {
    if (data_->released) {
      // We have been handed a half-destroyed object. The owner has already
      // marked it as `released` but the shared_ptr was still valid. Let's
      // release our copy right away.
      lock_.reset();
      data_.reset();
    }
  }
  Borrowed(const Borrowed&) = delete;
  Borrowed(Borrowed&&) = delete;
  Borrowed& operator=(Borrowed other) = delete;
  Borrowed& operator=(Borrowed&& other) = delete;

  bool Ok() const { return data_ != nullptr; }
  explicit operator bool() const { return Ok(); }
  T* operator->() {
    CHECK(Ok());
    return &data_->resource;
  }
  T& operator*() {
    CHECK(Ok());
    return data_->resource;
  }

 private:
  std::shared_ptr<BorrowableSharedData<T>> data_;
  std::unique_ptr<MutexLock> lock_;
};

// A weak reference to an object that can be borrowed.
template <typename T>
class Borrowable {
 public:
  explicit Borrowable(std::weak_ptr<BorrowableSharedData<T>> resource)
      : resource_(resource) {}

  // Gives the caller exclusive access to the stored object. Borrowing fails if
  // the object has already been destroyed. The call will block if another
  // thread is already borrowing the object.
  Borrowed<T> Borrow() const {
    std::shared_ptr<BorrowableSharedData<T>> data = resource_.lock();
    if (!data) {
      return Borrowed<T>();
    }
    return Borrowed<T>(data);
  }

 private:
  std::weak_ptr<BorrowableSharedData<T>> resource_;
};

// The owner of a resource that can be borrowed.
template <typename T>
class Lender {
 public:
  explicit Lender(T resource)
      : shared_data_(
            std::make_shared<BorrowableSharedData<T>>(std::move(resource))) {}
  ~Lender() { Release(); }

  Borrowable<T> GetBorrowable() { return Borrowable<T>(shared_data_); }

  // Destroys the stored resource. If the resource is currently borrowed, the
  // call will block until the resource is returned.
  // When the resource is released, calls to `Borrowable::Borrow()` will fail.
  void Release() {
    if (!shared_data_) {
      return;
    }
    {
      MutexLock lock(&shared_data_->mutex);
      // We can't destroy `shared_data_` while holding the mutex inside it.
      // Setting `released` fixes the race condition below.
      // Thread 1            | Thread 2
      // Lender::Release()   | Borrowable::Borrow()
      //                     | grabs shared_ptr<T>
      // locks mutex         |
      // sets `released`     |
      // unlocks mutex       |
      //                     | locks mutex
      // resets shared_data_ |
      //                     | `released` is set, so borrow fails - unlocks
      //                     |   mutex, deletes shared_ptr and returns invalid
      //                     |   `Borrowed` object.
      shared_data_->released = true;
    }
    shared_data_.reset();
  }

 private:
  std::shared_ptr<BorrowableSharedData<T>> shared_data_;
};

}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_INTERNAL_PLATFORM_BORROWABLE_H_
