#include "core/internal/offline_frames.h"
#include "platform/base/byte_array.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  location::nearby::ByteArray byte_array;
  byte_array.SetData(reinterpret_cast<const char*>(data), size);

  location::nearby::connections::parser::FromBytes(byte_array);

  return 0;
}
