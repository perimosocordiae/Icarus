#ifndef ICARUS_TYPE_GENERIC_STRUCT_H
#define ICARUS_TYPE_GENERIC_STRUCT_H

#include <vector>
#include "core/scope.h"
#include "module/module.h"
#include "type/callable.h"
#include "type/type.h"

namespace type {
struct GenericStruct : public Callable {
  TYPE_FNS(GenericStruct);
  GenericStruct(core::Scope const *scope, std::vector<Type const *> ts)
      : scope_(scope), mod_(scope->module()), deps_(std::move(ts)) {}

#include ICARUS_TYPE_VISITOR_METHODS

  module::BasicModule const *defining_module() const { return mod_; }

  core::Scope const *scope_       = nullptr;
  module::BasicModule const *mod_ = nullptr;
  std::vector<Type const *> deps_;
};

GenericStruct *GenStruct(core::Scope const *scope,
                         std::vector<Type const *> ts);
}  // namespace type

#endif  // ICARUS_TYPE_GENERIC_STRUCT_H
