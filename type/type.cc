#include "type/all.h"

#include "architecture.h"
#include "ast/declaration.h"
#include "ast/struct_literal.h"
#include "base/container/map.h"
#include "base/container/unordered_map.h"
#include "base/guarded.h"
#include "context.h"
#include "ir/arguments.h"
#include "ir/components.h"
#include "ir/func.h"
#include "ir/phi.h"
#include "module.h"

namespace type {
#define PRIMITIVE_MACRO(EnumName, name)                                        \
  Type const *EnumName = new Primitive(PrimType::EnumName);
#include "type/primitive.xmacro.h"
#undef PRIMITIVE_MACRO

Type const *Void() { return Tup({}); }

bool Type::is_big() const {
  return is<Array>() || is<Struct>() || is<Variant>() || is<Tuple>();
}

bool VerifyAssignment(TextSpan const &span, type::Type const *to,
                      type::Type const *from, Context *ctx) {
  // TODO this feels like the semantics are iffy. It works fine if we assign
  // to/from the same type, but we really care if you can assign to a type
  // rather than copy from another, I think.
  if (!from->IsMovable()) {
    ctx->error_log_.NotMovable(span, from);
    return false;
  }

  if (to == from) { return true; }
  auto *to_tup   = to->if_as<Tuple>();
  auto *from_tup = from->if_as<Tuple>();
  if (to_tup && from_tup) {
    if (to_tup->entries_.size() != from_tup->entries_.size()) {
      ctx->error_log_.MismatchedAssignmentSize(span, to_tup->entries_.size(),
                                               from_tup->entries_.size());
      return false;
    }

    bool result = true;
    for (size_t i = 0; i < to_tup->entries_.size(); ++i) {
      result &= VerifyAssignment(span, to_tup->entries_.at(i),
                                 from_tup->entries_.at(i), ctx);
    }
    return result;
  }

  if (auto *to_var = to->if_as<Variant>()) {
    if (auto *from_var = from->if_as<Variant>()) {
      for (auto fvar : from_var->variants_) {
        if (!to_var->contains(fvar)) {
          NOT_YET("log an error", from, to);
          return false;
        }
      }
      return true;
    } else {
      if (!to_var->contains(from)) {
        NOT_YET("log an error", from, to);
        return false;
      }

      return true;
    }
  }

  if (auto *to_ptr = to->if_as<Pointer>()) {
    if (from == NullPtr) { return true; }
    NOT_YET("log an error", from, to);
    return false;
  }

  NOT_YET("log an error: no cast from ", from, " to ", to);
}

void EmitCopyInit(Type const *from_type, ir::Val const &from_val,
                  Typed<ir::Register> to_var, Context *ctx) {
  auto *to_type = to_var.type()->as<Pointer>().pointee;
  // TODO Optimize once you understand the semantics better.
  if (!to_type->is<Primitive>() && !to_type->is<Function>() &&
      !to_type->is<Variant>()) {
    to_type->EmitInit(to_var.get(), ctx);
  }
  to_type->EmitCopyAssign(from_type, from_val, to_var.get(), ctx);
}

void EmitMoveInit(Type const *from_type, ir::Val const &from_val,
                  Typed<ir::Register> to_var, Context *ctx) {
  auto *to_type = to_var.type()->as<Pointer>().pointee;
  // TODO Optimize once you understand the semantics better.
  if (!to_type->is<Primitive>() && !to_type->is<Function>() &&
      !to_type->is<Variant>()) {
    to_type->EmitInit(to_var.get(), ctx);
  }
  to_type->EmitMoveAssign(from_type, from_val, to_var.get(), ctx);
}

}  // namespace type