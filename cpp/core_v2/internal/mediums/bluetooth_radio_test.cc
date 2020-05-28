#include "core_v2/internal/mediums/bluetooth_radio.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {
namespace {

TEST(BluetoothRadioTest, ConstructorDestructorWorks) {
  BluetoothRadio radio;
  EXPECT_TRUE(radio.IsAdapterValid());
}

TEST(BluetoothRadioTest, CanEnable) {
  BluetoothRadio radio;
  EXPECT_TRUE(radio.IsAdapterValid());
  EXPECT_FALSE(radio.IsEnabled());
  EXPECT_TRUE(radio.Enable());
  EXPECT_TRUE(radio.IsEnabled());
}

TEST(BluetoothRadioTest, CanDisable) {
  BluetoothRadio radio;
  EXPECT_TRUE(radio.IsAdapterValid());
  EXPECT_FALSE(radio.IsEnabled());
  EXPECT_TRUE(radio.Enable());
  EXPECT_TRUE(radio.IsEnabled());
  EXPECT_TRUE(radio.Disable());
  EXPECT_FALSE(radio.IsEnabled());
}

TEST(BluetoothRadioTest, CanToggle) {
  BluetoothRadio radio;
  EXPECT_TRUE(radio.IsAdapterValid());
  EXPECT_FALSE(radio.IsEnabled());
  EXPECT_TRUE(radio.Toggle());
  EXPECT_TRUE(radio.IsEnabled());
}

}  // namespace
}  // namespace connections
}  // namespace nearby
}  // namespace location
