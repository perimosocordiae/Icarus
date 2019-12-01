#ifndef ICARUS_COMPILER_DISPATCH_JUMP_TABLE_H
#define ICARUS_COMPILER_DISPATCH_JUMP_TABLE_H

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "ast/ast.h"
#include "base/expected.h"
#include "compiler/dispatch/overload.h"
#include "compiler/verify_result.h"
#include "core/fn_args.h"
#include "ir/jump.h"

namespace compiler {
struct Compiler;

struct JumpDispatchTable {
  static base::expected<JumpDispatchTable> Verify(
      Compiler *compiler, ast::ScopeNode const *node,
      absl::Span<ir::Jump const *const> jumps,
      core::FnArgs<VerifyResult> const &args);

 private:
  absl::flat_hash_map<ir::Jump const *, internal::ExprData> table_;
};

}  // namespace compiler

#endif  // ICARUS_COMPILER_DISPATCH_JUMP_TABLE_H