#include "core_v2/payload.h"

#include <memory>
#include <type_traits>

#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/input_stream.h"
#include "platform_v2/public/file.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace location {
namespace nearby {
namespace connections {

TEST(PayloadTest, DefaultPayloadHasUnknownType) {
  Payload payload;
  EXPECT_EQ(payload.GetType(), Payload::Type::kUnknown);
}

TEST(PayloadTest, SupportsByteArrayType) {
  const ByteArray bytes("bytes");
  Payload payload(bytes);
  EXPECT_EQ(payload.GetType(), Payload::Type::kBytes);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsBytes(), bytes);
}

TEST(PayloadTest, SupportsFileType) {
  InputFile* raw_file = new InputFile("/path/to/file", 0);
  std::unique_ptr<InputFile> file(raw_file);
  Payload payload(std::move(file));
  EXPECT_EQ(payload.GetType(), Payload::Type::kFile);
  EXPECT_EQ(payload.AsStream(), nullptr);
  EXPECT_EQ(payload.AsFile(), raw_file);
  EXPECT_EQ(payload.AsBytes(), ByteArray{});
}

TEST(PayloadTest, SupportsStreamType) {
  InputFile* raw_file = new InputFile("/path/to/file", 0);
  std::unique_ptr<InputStream> stream(raw_file);
  Payload payload(std::move(stream));
  EXPECT_EQ(payload.GetType(), Payload::Type::kStream);
  EXPECT_EQ(payload.AsStream(), raw_file);
  EXPECT_EQ(payload.AsFile(), nullptr);
  EXPECT_EQ(payload.AsBytes(), ByteArray{});
}

TEST(PayloadTest, PayloadIsMoveable) {
  Payload payload1;
  Payload payload2(ByteArray("bytes"));
  auto id = payload2.GetId();
  ByteArray bytes = payload2.AsBytes();
  EXPECT_EQ(payload1.GetType(), Payload::Type::kUnknown);
  EXPECT_EQ(payload2.GetType(), Payload::Type::kBytes);
  payload1 = std::move(payload2);
  EXPECT_EQ(payload1.GetType(), Payload::Type::kBytes);
  EXPECT_EQ(payload1.AsBytes(), bytes);
  EXPECT_EQ(payload1.GetId(), id);
}

TEST(PayloadTest, PayloadHasUniqueId) {
  Payload payload1;
  Payload payload2;
  EXPECT_NE(payload1.GetId(), payload2.GetId());
}

TEST(PayloadTest, PayloadIsNotCopyable) {
  EXPECT_FALSE(std::is_copy_constructible_v<Payload>);
  EXPECT_FALSE(std::is_copy_assignable_v<Payload>);
}

}  // namespace connections
}  // namespace nearby
}  // namespace location
