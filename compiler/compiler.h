#ifndef ICARUS_COMPILER_COMPILER_H
#define ICARUS_COMPILER_COMPILER_H

#include <memory>

#include "absl/container/flat_hash_map.h"
#include "ast/ast_fwd.h"
#include "ast/overload_set.h"
#include "ast/visitor.h"
#include "base/debug.h"
#include "base/move_func.h"
#include "compiler/constant_binding.h"
#include "compiler/data.h"
#include "compiler/dependent_data.h"
#include "compiler/verify_result.h"
#include "error/log.h"
#include "frontend/source/source.h"
#include "ir/addr.h"
#include "ir/builder.h"
#include "ir/reg.h"
#include "ir/results.h"
#include "module/module.h"
#include "type/type_fwd.h"
#include "type/visitor.h"

namespace ir {
struct CompiledFn;
struct ScopeDef;
struct BlockDef;
}  // namespace ir

namespace ast {
struct DispatchTable {
  // TODO obviously wrong but we're deprecating anyway
  ir::Results EmitInlineCall(
      compiler::Compiler *visitor,
      core::FnArgs<std::pair<Expression const *, ir::Results>> const &args,
      absl::flat_hash_map<ir::BlockDef const *, ir::BasicBlock *> const
          &block_map) const {
    return ir::Results{};
  }
  ir::Results EmitCall(
      compiler::Compiler *visitor,
      core::FnArgs<std::pair<Expression const *, ir::Results>> const &args,
      bool is_inline = false) const {
    return ir::Results{};
  }
};

inline compiler::VerifyResult VerifyJumpDispatch(
    void *visitor, ExprPtr expr,
    absl::Span<ir::JumpHandler const *const> overload_set,
    core::FnArgs<std::pair<Expression const *, compiler::VerifyResult>> const
        &args,
    std::vector<ir::BlockDef const *> *block_defs) {
  return compiler::VerifyResult::Error();
}
inline compiler::VerifyResult VerifyDispatch(
    void *visitor, ExprPtr expr, absl::Span<ir::AnyFunc const> overload_set,
    core::FnArgs<std::pair<Expression const *, compiler::VerifyResult>> const
        &args) {
  return compiler::VerifyResult::Error();
}
inline compiler::VerifyResult VerifyDispatch(
    void *visitor, ExprPtr expr, OverloadSet const &overload_set,
    core::FnArgs<std::pair<Expression const *, compiler::VerifyResult>> const
        &args) {
  return compiler::VerifyResult::Error();
}

}  // namespace ast

namespace compiler {
struct EmitRefTag {};
struct EmitCopyInitTag {};
struct EmitMoveInitTag {};
struct EmitValueTag {};
struct VerifyTypeTag {};
struct EmitPrintTag {};
struct EmitDestroyTag {};
struct EmitDefaultInitTag {};
struct EmitCopyAssignTag {};
struct EmitMoveAssignTag {};

std::unique_ptr<module::BasicModule> CompileModule(frontend::Source *src);

// These are the steps in a traditional compiler of verifying types and emitting
// code. They're tied together because they don't necessarily happen in a
// particular order. Certainly for any given AST node we need to verify its type
// before emitting code for it. However, we may need to emit and execute code
// for some nodes to compute a type at compile-time. For this reason these steps
// require the same contextual data and therefore should be placed into a simple
// visitor type.
//
// Note that tying these together has a cost. C++ ties together these steps as
// well as parsing and it has compile-time performance cost as well as language
// semantic restrictions. This was design was not chosen lightly. We believe
// that the primary problem with C++ is that parsing is lumped together with
// type-verification and code generation and that the primary benefits that
// surface from separation are from separating parsing from these two stages
// rather than separating all stages. In time we will see if this belief holds
// water.

struct Compiler
    : ast::Visitor<VerifyResult(VerifyTypeTag), ir::Results(EmitValueTag),
                   void(type::Typed<ir::Reg> reg, EmitMoveInitTag),
                   void(type::Typed<ir::Reg> reg, EmitCopyInitTag),
                   std::vector<ir::RegOr<ir::Addr>>(EmitRefTag)>,
      type::Visitor<void(ir::Results const &, EmitPrintTag),
                    void(ir::Reg reg, EmitDestroyTag),
                    void(ir::Reg reg, EmitDefaultInitTag),
                    void(ir::RegOr<ir::Addr>, type::Typed<ir::Results> const &,
                         EmitMoveAssignTag),
                    void(ir::RegOr<ir::Addr>, type::Typed<ir::Results> const &,
                         EmitCopyAssignTag)> {
  VerifyResult Visit(ast::Node const *node, VerifyTypeTag) {
    return ast::SingleVisitor<VerifyResult(VerifyTypeTag)>::Visit(
        node, VerifyTypeTag{});
  }

  ir::Results Visit(ast::Node const *node, EmitValueTag) {
    return ast::SingleVisitor<ir::Results(EmitValueTag)>::Visit(node,
                                                                EmitValueTag{});
  }

  void Visit(ast::Node const *node, type::Typed<ir::Reg> reg, EmitCopyInitTag) {
    ast::SingleVisitor<void(type::Typed<ir::Reg> reg, EmitCopyInitTag)>::Visit(
        node, reg, EmitCopyInitTag{});
  }

  void Visit(ast::Node const *node, type::Typed<ir::Reg> reg, EmitMoveInitTag) {
    ast::SingleVisitor<void(type::Typed<ir::Reg> reg, EmitMoveInitTag)>::Visit(
        node, reg, EmitMoveInitTag{});
  }

  std::vector<ir::RegOr<ir::Addr>> Visit(ast::Node const *node, EmitRefTag) {
    return ast::SingleVisitor<std::vector<ir::RegOr<ir::Addr>>(
        EmitRefTag)>::Visit(node, EmitRefTag{});
  }

  void Visit(type::Type const *t, ir::Results const &val, EmitPrintTag) {
    type::SingleVisitor<void(ir::Results const &, EmitPrintTag)>::Visit(
        t, val, EmitPrintTag{});
  }

  void Visit(type::Type const *t, ir::Reg r, EmitDestroyTag) {
    type::SingleVisitor<void(ir::Reg, EmitDestroyTag)>::Visit(t, r,
                                                              EmitDestroyTag{});
  }

  void Visit(type::Type const *t, ir::Reg r, EmitDefaultInitTag) {
    type::SingleVisitor<void(ir::Reg, EmitDefaultInitTag)>::Visit(
        t, r, EmitDefaultInitTag{});
  }

  void Visit(type::Type const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitMoveAssignTag) {
    type::SingleVisitor<void(
        ir::RegOr<ir::Addr>, type::Typed<ir::Results> const &,
        EmitMoveAssignTag)>::Visit(t, to, from, EmitMoveAssignTag{});
  }

  void Visit(type::Type const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitCopyAssignTag) {
    type::SingleVisitor<void(
        ir::RegOr<ir::Addr>, type::Typed<ir::Results> const &,
        EmitCopyAssignTag)>::Visit(t, to, from, EmitCopyAssignTag{});
  }

  Compiler(module::BasicModule *mod);

  module::BasicModule *module() { return data_.mod_; }
  ir::Builder &builder() { return data_.bldr_; };

  ir::CompiledFn MakeThunk(ast::Expression const *expr, type::Type const *type);

  // TODO Depending on if we're streaming or batching errors, we may want one
  // log per module, or one per compiler instance.
  error::Log *error_log() { return &data_.error_log_; }
  size_t num_errors() { return error_log()->size(); }
  void DumpErrors() { error_log()->Dump(); }

  VerifyResult const *prior_verification_attempt(ast::ExprPtr expr);
  type::Type const *type_of(ast::Expression const *expr) const;
  void set_addr(ast::Declaration const *decl, ir::Reg addr);
  compiler::VerifyResult set_result(ast::ExprPtr expr,
                                    compiler::VerifyResult r);

  ir::Reg addr(ast::Declaration const *decl) const;
  void set_dispatch_table(ast::ExprPtr expr, ast::DispatchTable &&table);
  void set_jump_table(ast::ExprPtr jump_expr, ast::ExprPtr node,
                      ast::DispatchTable &&table);

  ir::CompiledFn *AddFunc(
      type::Function const *fn_type,
      core::FnParams<type::Typed<ast::Declaration const *>> params);
  ir::CompiledFn *AddJump(
      type::Jump const *jump_type,
      core::FnParams<type::Typed<ast::Declaration const *>> params);

  ast::DispatchTable const *dispatch_table(ast::ExprPtr expr) const;
  ast::DispatchTable const *jump_table(ast::ExprPtr jump_expr,
                                       ast::ExprPtr node) const;

  module::PendingModule *pending_module(ast::Import const *import_node) const;

  std::pair<ConstantBinding, DependentData> *insert_constants(
      ConstantBinding const &constant_binding);

  void set_pending_module(ast::Import const *import_node,
                          module::PendingModule mod);

  void CompleteDeferredBodies();

  template <typename Fn>
  base::move_func<void()> *AddWork(ast::Node const *node, Fn &&fn) {
    auto [iter, success] =
        data_.deferred_work_.lock()->emplace(node, std::forward<Fn>(fn));
    ASSERT(success == true);
    return &iter->second;
  }

#define ICARUS_AST_NODE_X(name)                                                \
  ir::Results Visit(ast::name const *node, EmitValueTag);
#include "ast/node.xmacro.h"
#undef ICARUS_AST_NODE_X

#define ICARUS_AST_NODE_X(name)                                                \
  VerifyResult Visit(ast::name const *node, VerifyTypeTag);
#include "ast/node.xmacro.h"
#undef ICARUS_AST_NODE_X

  VerifyResult VerifyConcreteFnLit(ast::FunctionLiteral const *node);

  std::vector<ir::RegOr<ir::Addr>> Visit(ast::Access const *node, EmitRefTag);
  std::vector<ir::RegOr<ir::Addr>> Visit(ast::CommaList const *node,
                                         EmitRefTag);
  std::vector<ir::RegOr<ir::Addr>> Visit(ast::Identifier const *node,
                                         EmitRefTag);
  std::vector<ir::RegOr<ir::Addr>> Visit(ast::Index const *node, EmitRefTag);
  std::vector<ir::RegOr<ir::Addr>> Visit(ast::Unop const *node, EmitRefTag);

  void Visit(type::Array const *t, ir::Results const &val, EmitPrintTag);
  void Visit(type::Enum const *t, ir::Results const &val, EmitPrintTag);
  void Visit(type::Flags const *t, ir::Results const &val, EmitPrintTag);
  void Visit(type::Pointer const *t, ir::Results const &val, EmitPrintTag);
  void Visit(type::Primitive const *t, ir::Results const &val, EmitPrintTag);
  void Visit(type::Tuple const *t, ir::Results const &val, EmitPrintTag);
  void Visit(type::Variant const *t, ir::Results const &val, EmitPrintTag);

  void Visit(type::Struct const *t, ir::Reg reg, EmitDestroyTag);
  void Visit(type::Variant const *t, ir::Reg reg, EmitDestroyTag);
  void Visit(type::Tuple const *t, ir::Reg reg, EmitDestroyTag);
  void Visit(type::Array const *t, ir::Reg reg, EmitDestroyTag);

  void Visit(type::Array const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitCopyAssignTag);
  void Visit(type::Enum const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitCopyAssignTag);
  void Visit(type::Flags const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitCopyAssignTag);
  void Visit(type::Function const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitCopyAssignTag);
  void Visit(type::Pointer const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitCopyAssignTag);
  void Visit(type::Primitive const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitCopyAssignTag);
  void Visit(type::Struct const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitCopyAssignTag);
  void Visit(type::Tuple const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitCopyAssignTag);
  void Visit(type::Variant const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitCopyAssignTag);

  void Visit(type::Array const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitMoveAssignTag);
  void Visit(type::Enum const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitMoveAssignTag);
  void Visit(type::Flags const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitMoveAssignTag);
  void Visit(type::Function const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitMoveAssignTag);
  void Visit(type::Pointer const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitMoveAssignTag);
  void Visit(type::Primitive const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitMoveAssignTag);
  void Visit(type::Struct const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitMoveAssignTag);
  void Visit(type::Tuple const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitMoveAssignTag);
  void Visit(type::Variant const *t, ir::RegOr<ir::Addr> to,
             type::Typed<ir::Results> const &from, EmitMoveAssignTag);

  void Visit(type::Array const *t, ir::Reg reg, EmitDefaultInitTag);
  void Visit(type::Flags const *t, ir::Reg reg, EmitDefaultInitTag);
  void Visit(type::Pointer const *t, ir::Reg reg, EmitDefaultInitTag);
  void Visit(type::Primitive const *t, ir::Reg reg, EmitDefaultInitTag);
  void Visit(type::Struct const *t, ir::Reg reg, EmitDefaultInitTag);
  void Visit(type::Tuple const *t, ir::Reg reg, EmitDefaultInitTag);

  void Visit(ast::Expression const *, type::Typed<ir::Reg> reg,
             EmitMoveInitTag);
  void Visit(ast::ArrayLiteral const *, type::Typed<ir::Reg> reg,
             EmitMoveInitTag);
  void Visit(ast::CommaList const *, type::Typed<ir::Reg> reg, EmitMoveInitTag);
  void Visit(ast::Unop const *, type::Typed<ir::Reg> reg, EmitMoveInitTag);

  void EmitMoveInit(type::Type const *from_type, ir::Results const &from_val,
                    type::Typed<ir::Reg> to_var);

  void Visit(ast::Expression const *, type::Typed<ir::Reg> reg,
             EmitCopyInitTag);
  void Visit(ast::ArrayLiteral const *, type::Typed<ir::Reg> reg,
             EmitCopyInitTag);
  void Visit(ast::CommaList const *, type::Typed<ir::Reg> reg, EmitCopyInitTag);
  void Visit(ast::Unop const *, type::Typed<ir::Reg> reg, EmitCopyInitTag);

  void EmitCopyInit(type::Type const *from_type, ir::Results const &from_val,
                    type::Typed<ir::Reg> to_var);

  CompilationData data_;
};
}  // namespace compiler

#endif  // ICARUS_COMPILER_COMPILER_H