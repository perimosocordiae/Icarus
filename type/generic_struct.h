#ifndef ICARUS_TYPE_GENERIC_STRUCT_H
#define ICARUS_TYPE_GENERIC_STRUCT_H

#include <vector>
#include "misc/module.h"
#include "core/scope.h"
#include "type/callable.h"
#include "type/type.h"

struct Context;

namespace type {
struct GenericStruct : public Callable {
  TYPE_FNS(GenericStruct);
  GenericStruct(core::Scope const *scope, std::vector<Type const *> ts)
      : scope_(scope), mod_(scope->module()), deps_(std::move(ts)) {}

  void EmitDestroy(ir::Register reg, Context *ctx) const override;

  bool IsCopyable() const override;
  bool IsMovable() const override;

  ::Module const *defining_module() const { return mod_; }

  core::Scope const *scope_ = nullptr;
  ::Module const *mod_      = nullptr;
  std::vector<Type const *> deps_;
};

GenericStruct *GenStruct(core::Scope const *scope,
                         std::vector<Type const *> ts);
}  // namespace type

#endif  // ICARUS_TYPE_GENERIC_STRUCT_H
