#ifndef ICARUS_AST_CAST_H
#define ICARUS_AST_CAST_H

#include <memory>
#include <vector>

#include "ast/expression.h"

struct Context;

namespace ast {
struct Cast : public Expression {
  ~Cast() override {}

#include "ast_visitor/visitors.xmacro.h"

  std::string to_string(size_t n) const override;

  std::unique_ptr<Expression> expr_, type_;
};

}  // namespace ast

#endif  // ICARUS_AST_CAST_H
