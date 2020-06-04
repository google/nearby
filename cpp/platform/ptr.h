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

#ifndef PLATFORM_PTR_H_
#define PLATFORM_PTR_H_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

#include "platform/logging.h"
#include "platform/port/down_cast.h"

namespace location {
namespace nearby {

// Forward declarations to make it possible for Ptr (a class template) to
// declare ConstifyPtr, DowncastPtr, and DowncastConstPtr (function templates)
// as friends.
//
// Note that the default template parameters to Ptr need to be defined here (at
// the first point of declaration), as opposed to at the actual definition of
// Ptr (which is what one might reasonably expect).
//
// See https://isocpp.org/wiki/faq/templates#template-friends for more.
template <typename T>
class Ptr;
template <typename T>
class ConstPtr;
template <typename T>
ConstPtr<T> ConstifyPtr(Ptr<T> ptr);
template <typename ChildT, typename BaseT>
Ptr<ChildT> DowncastPtr(Ptr<BaseT> base_ptr);
template <typename ChildT, typename BaseT>
ConstPtr<ChildT> DowncastConstPtr(ConstPtr<BaseT> base_ptr);

// A layer of indirection over a raw pointer.
// It is being deprecated in favor of standard c++ smart pointers.
// For transion period, Ptr<T> will behave similar to shared_ptr<T>.
// New code should use shrared_ptr<T> or unique_ptr<T> and not Ptr<T>.
template <typename T>
class Ptr {
 public:
  // Provide an alias for use as a dependent name.
  typedef T PointeeType;

  Ptr() = default;
  explicit Ptr(T* pointee) : ptr_(pointee) {}
  Ptr(const Ptr& that) = default;
  Ptr(Ptr&& that) = default;

  Ptr(std::shared_ptr<T> ptr) : ptr_(ptr) {}  // NOLINT

  template <typename T2>
  Ptr<T>& operator=(T2* ptr) {
    Ptr<T> tmp(ptr);
    this->ptr_.swap(tmp);
    return *this;
  }

  Ptr& operator=(const Ptr& other) = default;

  // Conversion to Ptr<T2>, where T is trivially convertible to T2.  E.g.
  // conversion from derived to base class.
  template <typename T2>
  operator Ptr<T2>() {  // NOLINT
    return Ptr<T2>(std::static_pointer_cast<T2>(this->ptr_));
  }
  operator Ptr() {  // NOLINT
    return Ptr(*this);
  }

  explicit operator std::shared_ptr<T>() { return this->ptr_; }

  ~Ptr() = default;

  bool operator==(const Ptr& other) const {
    return *(this->ptr_) == *(other.ptr_);
  }
  bool operator!=(const Ptr& other) const { return !(*this == other); }

  bool operator<(const Ptr& other) const {
    return *(this->ptr_) < *(other.ptr_);
  }

  ABSL_DEPRECATED("Use c++ smart pointers directly instead of Ptr<T>")
  void destroy(bool = true) {
    // Legacy code expects isNull() to return true after destroy().
    ptr_.reset();
  }

  ABSL_DEPRECATED("Use c++ smart pointers directly instead of Ptr<T>")
  void clear() {
    // Legacy code expects isNull() to return true after clear().
    ptr_.reset();
  }

  T& operator*() const { return *ptr_; }

  T* operator->() const { return ptr_.get(); }
  T* get() { return ptr_.get(); }
  T* get() const { return ptr_.get(); }
  void reset() { return ptr_.reset(); }

  ABSL_DEPRECATED("Use c++ smart pointers directly instead of Ptr<T>")
  bool isNull() const { return !this->ptr_; }

  // used by pipe.cc; introduced by cr/295271652
  ABSL_DEPRECATED("Use c++ smart pointers directly instead of Ptr<T>")
  bool isRefCounted() const { return true; }

 private:
  template <typename PointeeT>
  friend ConstPtr<PointeeT> ConstifyPtr(Ptr<PointeeT> ptr);
  template <typename ChildT, typename BaseT>
  friend Ptr<ChildT> DowncastPtr(Ptr<BaseT> base_ptr);
  template <typename ChildT, typename BaseT>
  friend ConstPtr<ChildT> DowncastConstPtr(ConstPtr<BaseT> base_ptr);

  std::shared_ptr<T> ptr_;
};

// Convenience wrapper for a read-only version of Ptr (in which the pointee
// cannot be modified).
//
// The C++11 equivalent would be:
//
//      using ConstPtr = Ptr<T const>;
//
// Thus,
//
//      Ptr<X> x1(new X(...));
//
// allows the underlying X instance to be modified, whereas
//
//      ConstPtr<X> x2(new X(...));
//
// disallows that.
template <typename T>
class ConstPtr : public Ptr<T const> {
 public:
  ConstPtr() {}
  explicit ConstPtr(const T* pointee) : Ptr<T const>(pointee) {}
  explicit ConstPtr(T* pointee) : Ptr<T const>(pointee) {}
  explicit ConstPtr(Ptr<T> ptr) : Ptr<T const>(ptr) {}
};

// RAII wrapper over Ptr and ConstPtr (hereon referred to by the PtrType
// placeholder), to allow for guarantees that the wrapped PtrType will be
// automatically destroyed when this wrapper object goes out of scope.
//
// Any class that has a PtrType member that it owns (and thus needs to invoke
// destroy() on) should wrap that PtrType in a ScopedPtr object.
//
// Similarly, any method that manipulates a (likely local) PtrType variable
// that needs to be destroy()ed at the end of that method should wrap that
// PtrType variable in a ScopedPtr object.
//
// Sample usage:
//
//          Ptr<X> x1(new X(...));
//          ScopedPtr<Ptr<X> > sx1(x1);
//
//          ConstPtr<X> x2(new X(...));
//          ScopedPtr<ConstPtr<X> > sx2(x2);
//
//          ScopedPtr<Ptr<X> > sx3(new X(...));
//
//          ScopedPtr<ConstPtr<X> > sx4(new X(...));
template <typename PtrType>
class ScopedPtr {
 public:
  explicit ScopedPtr(typename PtrType::PointeeType* pointee) : ptr_(pointee) {}
  explicit ScopedPtr(PtrType ptr) : ptr_(ptr) {}
  ScopedPtr(const ScopedPtr&) = delete;
  ~ScopedPtr() = default;

  ScopedPtr& operator=(const ScopedPtr&) = delete;

  // Shadow methods for the underlying Ptr.
  typename PtrType::PointeeType& operator*() const { return *ptr_; }
  typename PtrType::PointeeType* operator->() const {
    return ptr_.operator->();
  }
  bool isNull() const { return ptr_.isNull(); }

  // Accessor for the underlying Ptr.
  PtrType get() const { return this->ptr_; }

  // TODO(b/149938110): remove this completely.
  PtrType release() {
    // Legacy code expects isNull() to return true after release().
    PtrType ptr = std::move(ptr_);
    ptr_.clear();
    return ptr;
  }

 private:
  PtrType ptr_;
};

// Utility function to create Ptr objects with less template-y noise by
// leveraging template argument deduction, in the same vein as std::make_pair().
//
// Helps convert
//
//      Ptr<MyRichType<MyTemplateParam> >(new MyRichType<MyTemplateParam>());
//
// to
//
//                                MakePtr(new MyRichType<MyTemplateParam>());
template <typename T>
Ptr<T> MakePtr(T* raw_ptr) {
  return Ptr<T>(raw_ptr);
}

// Like MakePtr(), utility function to create ConstPtr objects with less
// template-y noise.
template <typename T>
ConstPtr<T> MakeConstPtr(T* raw_ptr) {
  return ConstPtr<T>(raw_ptr);
}

// Used to create Ptr instances that are reference-counted (for when the
// lifetime and/or ownership of the pointee is not deterministic, like when a
// cache gives out handles to its cached objects to multiple threads to manage
// independently).
//
// Needless to say, the reference-counted-ness of these Ptr instances propagates
// across all copies and assignments, and as one might expect, the underlying
// pointee is deallocated when the reference count goes to 0.
//
// That implies that it's not strictly necessary to wrap these in ScopedPtrs
// (but it's perfectly fine to do so, and is even recommended, so readers of
// your code get a better understanding of the ownership story for each
// reference).
template <typename T>
Ptr<T> MakeRefCountedPtr(T* raw_ptr) {
  return Ptr<T>(raw_ptr);
}

// ConstPtr counterpart to MakeRefCountedPtr().
template <typename T>
ConstPtr<T> MakeRefCountedConstPtr(T* raw_ptr) {
  return ConstPtr<T>(raw_ptr);
}

// Use this function to convert a Ptr object to a ConstPtr object.
template <typename T>
ConstPtr<T> ConstifyPtr(Ptr<T> ptr) {
  return ConstPtr<T>(ptr);
}

// Use this function to downcast from a Ptr<BaseT> to a Ptr<ChildT>.
//
// Because BaseT can be automatically deduced based on the base_ptr that's
// passed in, invocations of this method only need to explicitly specify ChildT,
// like so:
//
//         Ptr<MyChild> my_child_ptr = DowncastPtr<MyChild>(my_base_ptr);
template <typename ChildT, typename BaseT>
Ptr<ChildT> DowncastPtr(Ptr<BaseT> base_ptr) {
  static_assert(std::is_base_of<BaseT, ChildT>::value,
                "Types do not share base class.");
  return Ptr<ChildT>(std::static_pointer_cast<ChildT>(base_ptr.ptr_));
}

// ConstPtr counterpart to DowncastPtr().
template <typename ChildT, typename BaseT>
ConstPtr<ChildT> DowncastConstPtr(ConstPtr<BaseT> base_ptr) {
  static_assert(std::is_base_of<BaseT, ChildT>::value,
                "Types do not share base class.");
  return ConstPtr<ChildT>(
      std::static_pointer_cast<const ChildT>(base_ptr.ptr_));
}

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PTR_H_
