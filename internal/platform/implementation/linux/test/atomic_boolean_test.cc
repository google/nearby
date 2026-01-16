#include "internal/platform/implementation/linux/atomic_boolean.h"

#include "gtest/gtest.h"

TEST(atomic_boolean, SuccessfulCreation) {
  // Arrange
  nearby::linux::AtomicBoolean atomicBoolean;
  bool oldValue = true;
  bool result = false;

  // Act
  oldValue = atomicBoolean.Set(true);
  result = atomicBoolean.Get();

  // Assert
  EXPECT_TRUE(result);
  EXPECT_FALSE(oldValue);
}
