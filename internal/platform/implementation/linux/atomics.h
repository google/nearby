// filepath: /workspace/internal/platform/implementation/linux/atomics.h
//
// Created by root on 10/2/25.
//
#ifndef LINUX_ATOMIC_BOOLEAN_H
#define LINUX_ATOMIC_BOOLEAN_H
#include  <atomic>
#include "internal/platform/implementation/atomic_boolean.h"
#include "internal/platform/implementation/atomic_reference.h"
namespace nearby
{
  namespace linux
  {
    // A boolean value that may be updated atomically.
    class AtomicBoolean : public api::AtomicBoolean
    {
    public:
      explicit AtomicBoolean(bool value = false) : atomic_boolean_(value)
      {
      }
      ~AtomicBoolean() override = default;
      // Atomically read and return current value.
      [[nodiscard]] bool Get() const override { return atomic_boolean_; };

      // Atomically exchange original value with a new one. Return previous value.
      bool Set(bool value) override { return atomic_boolean_.exchange(value); };

    private:
      std::atomic_bool atomic_boolean_ = false;
    };

    class AtomicUint32 : public api::AtomicUint32
    {
    public:
      explicit AtomicUint32(std::uint32_t value = 0) : atomic_uint32_(value) {}
      ~AtomicUint32() override = default;

      // Atomically reads and returns stored value.
      [[nodiscard]] std::uint32_t Get() const override { return atomic_uint32_.load(); }

      // Atomically stores value.
      void Set(std::uint32_t value) override { atomic_uint32_.store(value); }

    private:
      std::atomic<std::uint32_t> atomic_uint32_{0};
    };
  } // namespace api
} // namespace nearby
#endif //LINUX_ATOMIC_BOOLEAN_H
