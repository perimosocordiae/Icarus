#ifndef ICARUS_AST_COMMA_LIST_H
#define ICARUS_AST_COMMA_LIST_H

#include "ast/expression.h"

namespace ast {
struct CommaList : public Expression {
  CommaList() = default;
  ~CommaList() override {}

  CommaList(CommaList const &) noexcept = default;
  CommaList(CommaList &&) noexcept      = default;
  CommaList &operator=(CommaList const &) noexcept = default;
  CommaList &operator=(CommaList &&) noexcept = default;

  void assign_scope(core::Scope *scope) override;
  std::string to_string(size_t n) const override;
  VerifyResult VerifyType(Context *) override;
  void ExtractJumps(JumpExprs *) const override;
  void DependentDecls(DeclDepGraph *g,
                      Declaration *d) const override;
  bool InferType(type::Type const *t, InferenceState *state) const override {
    return false;
  }

  std::optional<std::vector<VerifyResult>> VerifyWithoutSetting(Context *ctx);

  bool needs_expansion() const override { return !parenthesized_; }

  ir::Results EmitIr(Context *) override;
  std::vector<ir::RegisterOr<ir::Addr>> EmitLVal(Context *) override;
  void EmitMoveInit(type::Typed<ir::Reg> reg, Context *ctx) override;
  void EmitCopyInit(type::Typed<ir::Reg> reg, Context *ctx) override;

  std::vector<std::unique_ptr<Expression>> exprs_;
};
}  // namespace ast

#endif  // ICARUS_AST_COMMA_LIST_H
