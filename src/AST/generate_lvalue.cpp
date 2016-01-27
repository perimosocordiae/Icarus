#include "AST.h"

extern llvm::Module* global_module;
extern llvm::IRBuilder<> builder;

namespace data {
  extern llvm::Value* const_uint(size_t n);
}  // namespace data

namespace AST {
  llvm::Value* Identifier::generate_lvalue(Scope* scope) {
    return alloc_;
  } 

  llvm::Value* Unop::generate_lvalue(Scope* scope) {
    if (op_ == Language::Operator::At) {
      return expr_->generate_code(scope);
    }

    return nullptr;
  }
  
  llvm::Value* ChainOp::generate_lvalue(Scope*)         { return nullptr; }
  llvm::Value* ArrayType::generate_lvalue(Scope*)       { return nullptr; }
  llvm::Value* ArrayLiteral::generate_lvalue(Scope*)    { return nullptr; }
  llvm::Value* Terminal::generate_lvalue(Scope*)        { return nullptr; }
  llvm::Value* FunctionLiteral::generate_lvalue(Scope*) { return nullptr; }
  llvm::Value* Case::generate_lvalue(Scope*)            { return nullptr; }
  llvm::Value* Assignment::generate_lvalue(Scope*)      { return nullptr; }
  llvm::Value* Declaration::generate_lvalue(Scope*)     { return nullptr; }
  llvm::Value* TypeLiteral::generate_lvalue(Scope*)     { return nullptr; }
  llvm::Value* EnumLiteral::generate_lvalue(Scope*)     { return nullptr; }

  llvm::Value* Binop::generate_lvalue(Scope* scope) {
    if (op_ == Language::Operator::Index && lhs_->type()->is_array()) {
      auto lhs_val = lhs_->generate_lvalue(scope);
      auto rhs_val = rhs_->generate_code(scope);
      auto load_ptr = scope->builder().CreateLoad(lhs_val);
      return scope->builder().CreateGEP(*type(), load_ptr, { rhs_val }, "array_idx");

    } else if (op_ == Language::Operator::Access) {
      // Automatically pass through pointers
      auto lhs_type = lhs_->type();
      auto lhs_lval = lhs_->generate_lvalue(scope);

      while (lhs_type->is_pointer()) {
        lhs_type = static_cast<Pointer*>(lhs_type)->pointee_type();
        lhs_lval = scope->builder().CreateLoad(lhs_lval);
      }

      auto udef_type = static_cast<UserDefined*>(lhs_type);
      return scope->builder().CreateGEP(lhs_lval,
          { data::const_uint(0), udef_type->field_num(rhs_->token()) });
    }

    return nullptr;
  }

}  // namespace AST
