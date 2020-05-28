#ifndef PLATFORM_V2_PUBLIC_FILE_H_
#define PLATFORM_V2_PUBLIC_FILE_H_

#include <cstdint>
#include <fstream>

#include "platform_v2/api/input_file.h"
#include "platform_v2/api/output_file.h"
#include "platform_v2/base/exception.h"
#include "absl/strings/string_view.h"

namespace location {
namespace nearby {

class InputFile final : public api::InputFile {
 public:
  explicit InputFile(const std::string& path, std::int64_t size);
  ~InputFile() override = default;
  InputFile(InputFile&&) = default;
  InputFile& operator=(InputFile&&) = default;

  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  std::string GetFilePath() const override { return path_; }
  std::int64_t GetTotalSize() const override { return total_size_; }
  Exception Close() override;

 private:
  std::ifstream file_;
  std::string path_;
  std::int64_t total_size_;
};

class OutputFile final : public api::OutputFile {
 public:
  explicit OutputFile(absl::string_view path);
  ~OutputFile() override = default;
  OutputFile(OutputFile&&) = default;
  OutputFile& operator=(OutputFile&&) = default;

  Exception Write(const ByteArray& data) override;
  Exception Flush() override;
  Exception Close() override;

 private:
  std::ofstream file_;
};

}  // namespace nearby
}  // namespace location

#endif  // PLATFORM_V2_PUBLIC_FILE_H_
