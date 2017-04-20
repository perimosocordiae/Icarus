#include "type/type.h"
#include "scope.h"
#include "ast/ast.h"

std::string Mangle(const Type *t, bool prefix) {
  if (t->is_primitive()) {
    if (t == Bool) { return "b"; }
    if (t == Char) { return "c"; }
    if (t == Int) { return "i"; }
    if (t == Real) { return "r"; }
    if (t == Uint) { return "u"; }
    if (t == Void) { return "v"; }
    if (t == Type_) { return "t"; }
    if (t == String) { return "s"; }
    std::cerr << *t << std::endl;
    UNREACHABLE;
  }

  std::stringstream ss;
  if (prefix) ss << "_Z";

  if (t->is_array()) {
    auto array_type = (const Array *)t;
    ss << 'A' << (array_type->fixed_length ? array_type->len : 0)
       << Mangle(array_type->data_type, false);

  } else if (t->is_pointer()) {
    ss << "P" << Mangle(((const Pointer *)t)->pointee, false);

  } else if (t->is_struct()) {
    auto struct_type = (const Struct *)t;
    ss << "S" << struct_type->bound_name.size() << struct_type->bound_name;

  } else if (t->is_function()) {
    // TODO treat as function pointer?
    ss << "F" << Mangle(((const Function *)t)->input, false);

  } else {
    ss << t->to_string();
  }

  return ss.str();
}

// TODO Mangle could just take a declaration and the type could be pulled out.
std::string Mangle(const Function *f, AST::Expression *expr,
                   Scope *starting_scope) {
  std::string name =
      expr->is_identifier() ? ((AST::Identifier *)expr)->token : "";

  if (expr->is_identifier()) {
    auto id = (AST::Identifier *)expr;
    if (id->decl->HasHashtag("cstdlib")) { return name; }
  }

  std::stringstream ss;
  ss << "_Z";

  // Use scopes in name mangling.
  // For now we're just concatenating pointers to the scopes which is really
  // ugly and makes the ABI impossible. But for now it uniques things correctly.
  // TODO To fix this, we need a way to assign names to scopes.
  auto scope = starting_scope ? starting_scope : expr->scope_;

  while (scope != Scope::Global) {
    ss << "X" << scope->name.size() << scope->name;
    scope = scope->parent;
  }

  ss << "F" << name.size() << name;
  ss << Mangle(f->input, false);
  return ss.str();
}
