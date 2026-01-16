#include "internal/platform/implementation/linux/atomic_reference.h"

#include "gtest/gtest.h"

TEST(atomic_reference, SuccessfulCreation) {
  // Arrange
  nearby::linux::AtomicUint32 atomicUint32;
  uint32_t result = UINT32_MAX;
  const uint32_t expected = 0;

  // Act
  result = atomicUint32.Get();

  // Assert
  EXPECT_EQ(result, expected);
}

TEST(atomic_reference, SuccessfulMaxSet) {
  // Arrange
  nearby::linux::AtomicUint32 atomicUint32;
  uint32_t result = 0;
  const uint32_t expected = UINT32_MAX;

  // Act
  atomicUint32.Set(UINT32_MAX);
  result = atomicUint32.Get();

  // Assert
  EXPECT_EQ(result, expected);
}

TEST(atomic_reference, SuccessfulMinSet) {
  // Arrange
  nearby::linux::AtomicUint32 atomicUint32;
  uint32_t result = UINT32_MAX;
  const uint32_t expected = 0;

  // Act
  atomicUint32.Set(0);
  result = atomicUint32.Get();

  // Assert
  EXPECT_EQ(result, expected);
}

TEST(atomic_reference, SetNegativeOneReturnsMAXUINT) {
  // Arrange
  nearby::linux::AtomicUint32 atomicUint32;
  uint32_t result = 0;
  const uint32_t expected = UINT32_MAX;

  // Act
  atomicUint32.Set(-1);  // Try Set -1, should actually store UINT32_MAX
  result = atomicUint32.Get();

  // Assert
  EXPECT_EQ(result, expected);
}
