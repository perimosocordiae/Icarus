#ifndef ICARUS_UNITY
#include "Scope.h"
#endif

namespace AST {
TokenNode::TokenNode(const Cursor &loc, const char *str_lit)
    : Node(loc), token(str_lit) {
#define OPERATOR_MACRO(name, symbol, prec, assoc)                              \
  if (strcmp(token, symbol) == 0) {                                            \
    op = Language::Operator::name;                                             \
    return;                                                                    \
  }
#include "config/operator.conf"
#undef OPERATOR_MACRO
  op = Language::Operator::NotAnOperator;
}

Expression::Expression()
    : precedence(Language::precedence(Language::Operator::NotAnOperator)),
      lvalue(Assign::Unset), type(nullptr), value(IR::Value::None()) {}

Declaration::Declaration()
    : identifier(nullptr), type_expr(nullptr), init_val(nullptr),
      addr(IR::Value::None()), arg_val(nullptr) {}

Generic::Generic() : test_fn(nullptr) {}
InDecl::InDecl() : container(nullptr) {}
ArrayLiteral::ArrayLiteral() {}
Access::Access() {}
ChainOp::ChainOp() {}
Case::Case() {}
Binop::Binop() {}
Unop::Unop() : operand(nullptr) {}
Terminal::Terminal() {}
ArrayType::ArrayType() {}

DummyTypeExpr::DummyTypeExpr() { UNREACHABLE; }

DummyTypeExpr::DummyTypeExpr(const Cursor &new_loc, Type *t) {
  loc   = new_loc;
  type  = Type_;
  value = IR::Value(t);
}

Jump::Jump(const Cursor &new_loc, JumpType jump_type) : jump_type(jump_type) {
  loc = new_loc;
}

Identifier::Identifier() { UNREACHABLE; }

Identifier::Identifier(const Cursor &new_loc, const std::string &token_string)
    : decl(nullptr) {
  token      = token_string;
  type       = nullptr;
  precedence = Language::precedence(Language::Operator::NotAnOperator);
  loc        = new_loc;
}

FunctionLiteral::FunctionLiteral()
    : fn_scope(new FnScope), return_type_expr(nullptr), llvm_fn(nullptr),
      statements(nullptr), ir_func(nullptr) {}

While::While() : while_scope(new BlockScope(ScopeEnum::While)) {}
For::For() : for_scope(new BlockScope(ScopeEnum::For)) {}

ScopeNode::ScopeNode()
    : scope_expr(nullptr), expr(nullptr), stmts(nullptr),
      internal(new BlockScope(ScopeEnum::Standard)) {}
} // namespace AST
