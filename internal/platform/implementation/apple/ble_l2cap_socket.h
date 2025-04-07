// Copyright 2025 Google LLC
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

#import <Foundation/Foundation.h>

#include <memory>

#import "internal/platform/implementation/apple/Mediums/BLEv2/GNCBLEL2CAPStream.h"
#include "internal/platform/implementation/ble_v2.h"

namespace nearby {
namespace apple {

/** A readable stream of bytes. */
class BleL2capInputStream : public InputStream {
 public:
  explicit BleL2capInputStream(GNCBLEL2CAPStream* stream);
  ~BleL2capInputStream() override = default;

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;

 private:
  GNCBLEL2CAPStream* stream_;
};

/** A writable stream of bytes. */
class BleL2capOutputStream : public OutputStream {
 public:
  explicit BleL2capOutputStream(GNCBLEL2CAPStream* stream);
  ~BleL2capOutputStream() override = default;

  Exception Write(const ByteArray& data) override;
  Exception Flush() override;
  Exception Close() override;

 private:
  GNCBLEL2CAPStream* stream_;
};

/**
 * Concrete BleL2capSocket implementation.
 */
class BleL2capSocket : public api::ble_v2::BleL2capSocket {
 public:
  explicit BleL2capSocket(GNCBLEL2CAPStream* stream);
  ~BleL2capSocket() override = default;

  InputStream& GetInputStream() override;
  OutputStream& GetOutputStream() override;
  void SetCloseNotifier(absl::AnyInvocable<void()> notifier) override;
  Exception Close() override;

 private:
  GNCBLEL2CAPStream* stream_;
  std::unique_ptr<BleL2capInputStream> input_stream_;
  std::unique_ptr<BleL2capOutputStream> output_stream_;
};

}  // namespace apple
}  // namespace nearby