#include <filesystem>
#include <vector>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "ast/node.h"
#include "format/token_extractor.h"
#include "frontend/parse.h"
#include "frontend/source/string.h"
#include "init/cli.h"

namespace format {
int FormatFile(std::filesystem::path const &file) {
  frontend::StringSource src("3 + abc");
  auto stmts = frontend::Parse(&src);
  TokenExtractor visitor;
  for (auto const &stmt : stmts) { visitor.Visit(stmt.get()); }
  return 0;
}
}  // namespace format

void cli::Usage() {
  Flag("help") << "Show usage information." << [] { execute = cli::ShowUsage; };

  // TODO error-out if more than one file is provided
  static char const *file;
  HandleOther = [](char const *arg) { file = arg; };
  execute     = [] { return format::FormatFile(std::filesystem::path{file}); };
}

int main(int argc, char *argv[]) {
  absl::InitializeSymbolizer(argv[0]);
  absl::FailureSignalHandlerOptions opts;
  absl::InstallFailureSignalHandler(opts);
  return cli::ParseAndRun(argc, argv);
}