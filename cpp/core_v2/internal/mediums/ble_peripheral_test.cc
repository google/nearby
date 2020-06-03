#include "core_v2/internal/mediums/ble_peripheral.h"

#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace mediums {
namespace {

const char kId[] = "AB12";

TEST(BlePeripheralTest, ConstructionWorks) {
  ByteArray id{kId};

  BlePeripheral ble_peripheral{id};

  EXPECT_TRUE(ble_peripheral.IsValid());
  EXPECT_EQ(id, ble_peripheral.GetId());
}

TEST(BlePeripheralTest, ConstructionEmptyFails) {
  BlePeripheral ble_peripheral;

  EXPECT_FALSE(ble_peripheral.IsValid());
  EXPECT_TRUE(ble_peripheral.GetId().Empty());
}

}  // namespace
}  // namespace mediums
}  // namespace connections
}  // namespace nearby
}  // namespace location
