#include "type.h"

#include "../architecture.h"
#include "../ast/ast.h"
#include "../ir/ir.h"
#include "scope.h"

extern IR::Val Evaluate(AST::Expression *expr);

Struct::Struct(const std::string &name) : bound_name(name) {}

void Struct::CompleteDefinition() {
  if (completed_) { return; }
  if (!field_num_to_name.empty()) { return; }

  for (size_t i = 0; i < decls.size(); ++i) {
    std::vector<Error> errors;
    decls[i]->verify_types(&errors);

    Type *decl_type;
    if (decls[i]->type_expr) {
      if (decls[i]->type_expr->type == Err ||
          decls[i]->type_expr->type == Void ||
          decls[i]->type_expr->type->is<ParamStruct>()) {
        decl_type = Err;
      } else {
        decl_type = Evaluate(decls[i]->type_expr.get()).as_type;
      }
    } else {
      decl_type = decls[i]->init_val->type;
    }

    insert_field(decls[i]->identifier->token, decl_type, decls[i]->init_val.get());
  }
  completed_ = true;
}

Type *Struct::field(const std::string &name) const {
  auto iter = field_name_to_num.find(name);
  return (iter == field_name_to_num.end()) ? nullptr
                                           : field_type AT(iter->second);
}

size_t Struct::field_num(const std::string &name) const {
  auto iter = field_name_to_num.find(name);
  ASSERT(iter != field_name_to_num.end(), "");
  return iter->second;
}

void Struct::insert_field(const std::string &name, Type *ty,
                             AST::Expression *init_val) {
  auto next_num           = field_num_to_name.size();
  field_name_to_num[name] = next_num;
  field_num_to_name.push_back(name);
  field_type.push_back(ty);

  { // Check sizes align
    size_t size1 = field_name_to_num.size();
    size_t size2 = field_num_to_name.size();
    size_t size3 = field_type.size();
    ASSERT(size1 == size2 && size2 == size3,
           "Size mismatch in struct database");
  }

  // By default, init_val is nullptr;
  init_values.emplace_back(init_val);
}

std::string Struct::to_string() const { return bound_name; }

Struct Struct::Anon(const std::set<AST::Declaration *> &declarations) {
  static int counter = 0;
  Struct result("anon.struct." + std::to_string(counter++));
  for (auto decl : declarations) {
    result.insert_field(decl->identifier->token, decl->type, nullptr);
  }
  result.CompleteDefinition();
  return result;
}