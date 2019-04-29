#ifndef ICARUS_AST_BLOCK_LITERAL_H
#define ICARUS_AST_BLOCK_LITERAL_H

#include "ast/declaration.h"
#include "ast/literal.h"

namespace ast {
struct BlockLiteral : public Literal {
  BlockLiteral(bool required);
  ~BlockLiteral() override {}
  std::string to_string(size_t n) const override;
  void assign_scope(core::Scope *scope) override;
  VerifyResult VerifyType(Context *) override;
  void ExtractJumps(JumpExprs *) const override;
  void DependentDecls(DeclDepGraph *g,
                      Declaration *d) const override;
  bool InferType(type::Type const *t, InferenceState *state) const override {
    return false;
  }

  ir::Results EmitIr(Context *ctx);

  std::vector<Declaration> before_, after_;
  std::unique_ptr<core::Scope> body_scope_;
  bool required_;
};
}  // namespace ast

#endif  // ICARUS_AST_BLOCK_LITERAL_H
