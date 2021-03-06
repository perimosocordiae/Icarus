#include "frontend/source/file.h"

#include <cerrno>
#include <cstring>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"

namespace frontend {

absl::StatusOr<FileSource> FileSource::Make(CanonicalFileName file_name) {
  auto f = file_name.OpenReadOnly();
  if (not f) {
    return absl::NotFoundError(
        absl::StrFormat(R"(Failed to open file "%s": %s)", file_name.name(),
                        std::strerror(errno)));
  }

  std::fseek(f.get(), 0, SEEK_END);
  size_t file_size = std::ftell(f.get());
  std::rewind(f.get());

  // TODO: Filling this buffer shouldn't be necessary.
  std::string s(file_size, '\0');
  std::fread(s.data(), sizeof(char), file_size, f.get());
  return FileSource(std::move(file_name), std::move(s));
}

}  // namespace frontend
