#ifndef PLATFORM_PTR_H_
#define PLATFORM_PTR_H_

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "platform/impl/default/default_lock.h"
#include "platform/logging.h"
#include "platform/port/down_cast.h"

namespace location {
namespace nearby {

namespace ptr_impl {

class RefCount {
 public:
  RefCount() : lock_(), count_(kInitialCount) {}

  // Returns false if this operation doesn't make conceptual sense any more
  // (for example, if it leads to bringing count_ back from the dead).
  bool increment() {
    bool result;

    lock_.lock();
    {
      // Avoid coming back from the dead.
      if (count_ < kInitialCount) {
        result = false;
      } else {
        count_++;
        result = true;
      }
    }
    lock_.unlock();

    return result;
  }

  // Returns true if after this operation, count_ is 0.
  bool decrement() {
    bool result;

    lock_.lock();
    {
      // It's alright for count_ to go negative because it will only be exactly
      // 0 once (since increment() makes sure that once you go negative, you
      // can't come back from the dead).
      count_--;
      result = (count_ == 0);
    }
    lock_.unlock();

    return result;
  }

 private:
  static const std::int32_t kInitialCount;

  DefaultLock lock_;
  std::int32_t count_;
};

}  // namespace ptr_impl

template <typename T>
class ObjectDestroyer {
 public:
  static void destroy(T* t) { delete t; }
};

template <typename T>
class ArrayDestroyer {
 public:
  static void destroy(T* t) { delete[] t; }
};

// Forward declarations to make it possible for Ptr (a class template) to
// declare ConstifyPtr, DowncastPtr, and DowncastConstPtr (function templates)
// as friends.
//
// Note that the default template parameters to Ptr need to be defined here (at
// the first point of declaration), as opposed to at the actual definition of
// Ptr (which is what one might reasonably expect).
//
// See https://isocpp.org/wiki/faq/templates#template-friends for more.
template <typename T, template <typename> class Destroyer = ObjectDestroyer>
class Ptr;
template <typename T>
class ConstPtr;
template <typename T>
ConstPtr<T> ConstifyPtr(Ptr<T> ptr);
template <typename ChildT, typename BaseT>
Ptr<ChildT> DowncastPtr(Ptr<BaseT> base_ptr);
template <typename ChildT, typename BaseT>
ConstPtr<ChildT> DowncastConstPtr(ConstPtr<BaseT> base_ptr);

// A layer of indirection over a raw pointer, to buy flexibility in the
// future to use, for instance:
//
// a) the in-built shared_ptr in modern implementations of C++,
// b) a custom reference-counting mechanism, etc.
//
// , all without having to touch every line of our codebase that uses
// pointers.
//
// Destroyer defines how the owned pointee should be destroyed, and is
// expected to be a class template that provides at least a destroy()
// method, like so:
//
// template <typename T>
// class MyDestroyer {
// public:
//   static void destroy(T* t);
// };
//
// It defaults to ObjectDestroyer<T>.
template <typename T, template <typename> class Destroyer>
class Ptr {
 public:
  // Provide an alias for use as a dependent name.
  typedef T PointeeType;

  Ptr() : pointee_(nullptr), ref_count_(nullptr) {}
  explicit Ptr(T* pointee, bool is_ref_counted = false,
               ptr_impl::RefCount* ref_count = nullptr)
      : pointee_(pointee),
        ref_count_(
            is_ref_counted
                ? (ref_count != nullptr ? ref_count : new ptr_impl::RefCount())
                : nullptr) {
    init();
  }
  Ptr(const Ptr& that) : pointee_(that.pointee_), ref_count_(that.ref_count_) {
    init();
  }

  Ptr& operator=(const Ptr& other) {
    if (pointee_ != other.pointee_) {
      // If we're not currently ref-counted, then an assignment shouldn't lead
      // to any destruction of our past state -- that's the responsibility of
      // whichever instance of Ptr believes it owns pointee_.
      destroy(false);

      pointee_ = other.pointee_;
      ref_count_ = other.ref_count_;

      init();
    }
    return *this;
  }

  // Conversion to Ptr<T2>, where T is trivially convertible to T2.  E.g.
  // conversion from derived to base class.
  template <typename T2>
  operator Ptr<T2>() {
    return Ptr<T2>(pointee_, isRefCounted(), ref_count_);
  }

  ~Ptr() {
    if (isRefCounted()) {
      destroy();
    } else {
      // Left empty on purpose.
    }
  }

  bool operator==(const Ptr& other) const {
    assert(!(this->isNull()));
    assert(!(other.isNull()));

    return ((*(this->pointee_) == *(other.pointee_)) &&
            (this->isRefCounted() == other.isRefCounted()));
  }

  bool operator!=(const Ptr& other) const { return !(*this == other); }

  bool operator<(const Ptr& other) const {
    assert(!(this->isNull()));
    assert(!(other.isNull()));

    return *(this->pointee_) < *(other.pointee_);
  }

  // Calls Destroyer::destroy() to perform deallocation of pointee_.
  void destroy(bool should_destroy_if_not_ref_counted = true) {
    bool need_to_destroy = isRefCounted() ? ref_count_->decrement()
                                          : should_destroy_if_not_ref_counted;
    if (need_to_destroy) {
      delete ref_count_;
      Destroyer<T>::destroy(pointee_);
    }

    ref_count_ = NULL;  // NOLINT
    pointee_ = NULL;    // NOLINT
  }

  // Use this function only when the ownership is held by someone else, and this
  // Ptr object has no responsibility to destroy it.
  void clear() {
    if (isRefCounted()) {
      NEARBY_LOG(FATAL, "Attempting to invoke clear() on a RefCounted Ptr.");
    }

    pointee_ = NULL;  // NOLINT
  }

  T& operator*() const {
    assert(pointee_ != NULL);  // NOLINT
    return *pointee_;
  }

  T* operator->() const {
    assert(pointee_ != NULL);  // NOLINT
    return pointee_;
  }

  bool isNull() const { return pointee_ == nullptr; }
  bool isRefCounted() const { return ref_count_ != nullptr; }

 private:
  template <typename PointeeT>
  friend ConstPtr<PointeeT> ConstifyPtr(Ptr<PointeeT> ptr);
  template <typename ChildT, typename BaseT>
  friend Ptr<ChildT> DowncastPtr(Ptr<BaseT> base_ptr);
  template <typename ChildT, typename BaseT>
  friend ConstPtr<ChildT> DowncastConstPtr(ConstPtr<BaseT> base_ptr);

  void init() {
    if (isRefCounted()) {
      if (!ref_count_->increment()) {
        NEARBY_LOG(FATAL, "Failed to increment RefCount.");
      }
    }
  }

  T* pointee_;
  ptr_impl::RefCount* ref_count_;
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
  explicit ConstPtr(T* pointee, bool is_ref_counted = false,
                    ptr_impl::RefCount* ref_count = nullptr)
      : Ptr<T const>(pointee, is_ref_counted, ref_count) {}
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
  ~ScopedPtr() { ptr_.destroy(); }

  // Shadow methods for the underlying Ptr.
  typename PtrType::PointeeType& operator*() const { return ptr_.operator*(); }
  typename PtrType::PointeeType* operator->() const {
    return ptr_.operator->();
  }
  bool isNull() const { return ptr_.isNull(); }

  // Accessor for the underlying Ptr.
  PtrType get() const { return ptr_; }

  // Releases the underlying Ptr from the clutches of this ScopedPtr,
  // effectively resetting this ScopedPtr (and making its destructor be a no-op)
  // -- useful for transfer of ownership from one ScopedPtr to another across
  // scopes.
  PtrType release() {
    PtrType released = ptr_;
    ptr_ = PtrType();
    return released;
  }

 private:
  // Disallow copy and assignment.
  ScopedPtr(const ScopedPtr&);
  ScopedPtr& operator=(const ScopedPtr&);

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
  return Ptr<T>(raw_ptr, true);
}

// ConstPtr counterpart to MakeRefCountedPtr().
template <typename T>
ConstPtr<T> MakeRefCountedConstPtr(T* raw_ptr) {
  return ConstPtr<T>(raw_ptr, true);
}

// Use this function to convert a Ptr object to a ConstPtr object.
template <typename T>
ConstPtr<T> ConstifyPtr(Ptr<T> ptr) {
  return ConstPtr<T>(ptr.pointee_, ptr.isRefCounted(), ptr.ref_count_);
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
  return Ptr<ChildT>(DOWN_CAST<ChildT*>(base_ptr.pointee_),
                     base_ptr.isRefCounted(), base_ptr.ref_count_);
}

// ConstPtr counterpart to DowncastPtr().
template <typename ChildT, typename BaseT>
ConstPtr<ChildT> DowncastConstPtr(ConstPtr<BaseT> base_ptr) {
  return ConstPtr<ChildT>(
      const_cast<ChildT*>(DOWN_CAST<const ChildT*>(base_ptr.pointee_)),
      base_ptr.isRefCounted(), base_ptr.ref_count_);
}

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_PTR_H_
