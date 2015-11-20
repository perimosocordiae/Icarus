#include "AST.h"

namespace AST {
  void Unop::join_identifiers(Scope* scope) {
    if (expr_->is_identifier()) {
      expr_ = scope->identifier(expr_);
    } else {
      expr_->join_identifiers(scope);
    }
  }

  void While::join_identifiers(Scope* scope) {
    cond_->join_identifiers(body_scope_);
    statements_->join_identifiers(body_scope_);
  }

  void Conditional::join_identifiers(Scope* scope) {
    cond_->join_identifiers(body_scope_);
    statements_->join_identifiers(body_scope_);
  }

  void Binop::join_identifiers(Scope* scope) {
    if (lhs_->is_identifier()) {
      lhs_ = scope->identifier(lhs_);
    } else {
      lhs_->join_identifiers(scope);
    }

    if (rhs_->is_identifier()) {
      rhs_ = scope->identifier(rhs_);
    } else {
      rhs_->join_identifiers(scope);
    }
  }

  void ArrayType::join_identifiers(Scope* scope) {
    if (len_->is_identifier()) {
      len_ = scope->identifier(len_);
    } else {
      len_->join_identifiers(scope);
    }

    if (array_type_->is_identifier()) {
      array_type_ = scope->identifier(array_type_);
    } else {
      array_type_->join_identifiers(scope);
    }
  }


  void Declaration::join_identifiers(Scope* scope) {
    id_ = std::static_pointer_cast<Identifier>(
        scope->identifier(declared_identifier()));
    id_->line_num_ = line_num_;

    if (decl_type_->is_identifier()) {
      decl_type_ = scope->identifier(decl_type_);
    } else {
      decl_type_->join_identifiers(scope);
    }
  }

  void ChainOp::join_identifiers(Scope* scope) {
    for (auto& expr : exprs_) {
      if (expr->is_identifier()) {
        expr = scope->identifier(expr);
      } else {
        expr->join_identifiers(scope);
      }
    }
  }

  void Case::join_identifiers(Scope* scope) {
    pairs_->join_identifiers(scope);
  }

  void KVPairList::join_identifiers(Scope* scope) {
    for (auto& pair : kv_pairs_) {
      if (pair.first->is_identifier()) {
        pair.first = scope->identifier(pair.first);
      } else {
        pair.first->join_identifiers(scope);
      }

      if (pair.second->is_identifier()) {
        pair.second = scope->identifier(pair.second);
      } else {
        pair.second->join_identifiers(scope);
      }
    }
  }

  void Statements::join_identifiers(Scope* scope) {
    for (auto& ptr : statements_) {
      if (ptr->is_identifier()) {
        ptr = std::static_pointer_cast<Node>(
            scope->identifier(
              std::static_pointer_cast<Expression>(ptr)
              )
            );
      } else {
        ptr->join_identifiers(scope);
      }
    }
  }

  void FunctionLiteral::join_identifiers(Scope* scope) {
    for (auto& in : inputs_) {
      in->join_identifiers(fn_scope_);
    }
    statements_->join_identifiers(fn_scope_);
  }

}  // namespace AST
