#include "compiler/compiler.h"

#include "ast/ast.h"

#include "backend/eval.h"
#include "ir/addr.h"
#include "ir/cmd/cast.h"
#include "ir/cmd/load.h"
#include "ir/cmd/misc.h"
#include "ir/components.h"
#include "ir/str.h"
#include "type/type.h"
#include "type/typed_value.h"

namespace compiler {
using ::matcher::InheritsFrom;

std::vector<ir::RegOr<ir::Addr>> Compiler::Visit(ast::Access const *node,
                                                 EmitRefTag) {
  auto reg = Visit(node->operand(), EmitRefTag{})[0];
  auto *t  = type_of(node->operand());

  while (auto *tp = t->if_as<type::Pointer>()) {
    t   = tp->pointee;
    reg = ir::Load<ir::Addr>(reg);
  }

  ASSERT(t, InheritsFrom<type::Struct>());
  auto *struct_type = &t->as<type::Struct>();
  return {ir::Field(reg, struct_type, struct_type->index(node->member_name()))
              .get()};
}

std::vector<ir::RegOr<ir::Addr>> Compiler::Visit(ast::CommaList const *node,
                                                 EmitRefTag) {
  std::vector<ir::RegOr<ir::Addr>> results;
  results.reserve(node->exprs_.size());
  for (auto &expr : node->exprs_) {
    results.push_back(Visit(expr.get(), EmitRefTag{})[0]);
  }
  return results;
}

std::vector<ir::RegOr<ir::Addr>> Compiler::Visit(ast::Identifier const *node,
                                                 EmitRefTag) {
  ASSERT(node->decl() != nullptr);
  return {addr(node->decl())};
}

std::vector<ir::RegOr<ir::Addr>> Compiler::Visit(ast::Index const *node,
                                                 EmitRefTag) {
  auto *lhs_type = type_of(node->lhs());
  auto *rhs_type = type_of(node->rhs());

  if (lhs_type->is<type::Array>()) {
    auto index = ir::CastTo<int64_t>(rhs_type, Visit(node->rhs(), EmitValueTag{}));

    auto lval = Visit(node->lhs(), EmitRefTag{})[0];
    if (not lval.is_reg()) { NOT_YET(this, type_of(node)); }
    return {ir::Index(type::Ptr(type_of(node->lhs())), lval.reg(), index)};
  } else if (auto *buf_ptr_type = lhs_type->if_as<type::BufferPointer>()) {
    auto index = ir::CastTo<int64_t>(rhs_type, Visit(node->rhs(), EmitValueTag{}));

    return {ir::PtrIncr(Visit(node->lhs(), EmitValueTag{}).get<ir::Reg>(0), index,
                        type::Ptr(buf_ptr_type->pointee))};
  } else if (lhs_type == type::ByteView) {
    // TODO interim until you remove string_view and replace it with Addr
    // entirely.
    auto index = ir::CastTo<int64_t>(rhs_type, Visit(node->rhs(), EmitValueTag{}));
    return {ir::PtrIncr(
        ir::GetString(
            Visit(node->lhs(), EmitValueTag{}).get<std::string_view>(0).value()),
        index, type::Ptr(type::Nat8))};
  } else if (auto *tup = lhs_type->if_as<type::Tuple>()) {
    auto index = ir::CastTo<int64_t>(
                     rhs_type, backend::Evaluate(
                                   type::Typed{node->rhs(), rhs_type}, this))
                     .value();
    return {ir::Field(Visit(node->lhs(), EmitRefTag{})[0], tup, index).get()};
  }
  UNREACHABLE(*this);
}

std::vector<ir::RegOr<ir::Addr>> Compiler::Visit(ast::Unop const *node,
                                                 EmitRefTag) {
  ASSERT(node->op() == frontend::Operator::At);
  return {Visit(node->operand(), EmitValueTag{}).get<ir::Reg>(0)};
}

}  // namespace compiler
