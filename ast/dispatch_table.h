#ifndef ICARUS_AST_DISPATCH_TABLE_H
#define ICARUS_AST_DISPATCH_TABLE_H

#include <string>
#include <variant>

#include "absl/container/flat_hash_map.h"
#include "ast/overload_set.h"
#include "core/fn_args.h"
#include "ir/block.h"
#include "type/typed_value.h"

struct Context;

namespace type {
struct Type;
}  // namespace type

namespace ast {
struct Node;
struct Expression;
struct ExprPtr;

struct DispatchTable {
  struct Row {
    Row(core::FnParams<type::Typed<ast::Expression *>> p,
        type::Function const *t, std::variant<Expression *, ir::AnyFunc> f)
        : params(std::move(p)), type(t), fn(std::move(f)) {}

    // In the typed-expression, each expression may be null (if no default value
    // is possible), but the type will always be present.
    core::FnParams<type::Typed<Expression *>> params;
    type::Function const *type;
    std::variant<Expression *, ir::AnyFunc> fn;
  };

  static std::pair<DispatchTable, type::Type const *> Make(
      core::FnArgs<type::Typed<Expression *>> const &args,
      OverloadSet const &overload_set, Context *ctx);

  ir::Results EmitInlineCall(
      core::FnArgs<std::pair<ast::Expression *, ir::Results>> const &args,
      type::Type const *,
      absl::flat_hash_map<ir::Block, ir::BlockIndex> const &block_map,
      Context *ctx) const;
  ir::Results EmitCall(
      core::FnArgs<std::pair<ast::Expression *, ir::Results>> const &args,
      type::Type const *, Context *ctx) const;

  std::vector<Row> bindings_;
  std::vector<type::Type const*> return_types_;
};

VerifyResult VerifyDispatch(
    ExprPtr expr, OverloadSet const &os,
    core::FnArgs<std::pair<Expression *, VerifyResult>> const &args,
    Context *ctx);

}  // namespace ast

#endif  // ICARUS_AST_DISPATCH_TABLE_H
