#ifndef ICARUS_REPL_MODULE_H
#define ICARUS_REPL_MODULE_H

#include "ast/ast.h"
#include "compiler/compiler.h"
#include "compiler/module.h"

namespace repl {

struct Module : compiler::CompiledModule {
  ~Module() override {}

  void ProcessNodes(base::PtrSpan<ast::Node const> nodes,
                    diagnostic::DiagnosticConsumer &diag,
                    module::Importer &importer) override;
};

}  // namespace repl

#endif  // ICARUS_REPL_MODULE_H
