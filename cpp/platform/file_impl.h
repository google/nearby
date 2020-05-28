#ifndef PLATFORM_FILE_IMPL_H_
#define PLATFORM_FILE_IMPL_H_

#include <cstdint>
#include <fstream>

#include "platform/api/input_file.h"
#include "platform/api/output_file.h"
#include "platform/exception.h"
#include "platform/ptr.h"

namespace location {
namespace nearby {

class InputFileImpl final : public InputFile {
 public:
  InputFileImpl(const std::string& path, std::int64_t size);
  ~InputFileImpl() override {}

  ExceptionOr<ConstPtr<ByteArray>> read(std::int64_t size) override;
  std::string getFilePath() const override;
  std::int64_t getTotalSize() const override;
  void close() override;

 private:
  std::ifstream file_;
  const std::string path_;
  const std::int64_t total_size_;
};

class OutputFileImpl final : public OutputFile {
 public:
  explicit OutputFileImpl(const std::string& path);
  ~OutputFileImpl() override {}

  Exception::Value write(ConstPtr<ByteArray> data) override;
  void close() override;

 private:
  std::ofstream file_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_FILE_IMPL_H_
