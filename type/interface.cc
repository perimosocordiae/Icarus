#include "type/interface.h"

#include <mutex>
#include <utility>

#include "base/guarded.h"
#include "ir/arguments.h"
#include "ir/components.h"
#include "ir/compiled_fn.h"
#include "core/arch.h"
#include "misc/context.h"
#include "misc/module.h"
#include "type/function.h"
#include "type/pointer.h"

namespace type {

void Interface::defining_modules(
    absl::flat_hash_set<::Module const *> *modules) const {
  modules->insert(defining_module());
}

bool Interface::matches(Type const *t) const {
  // TODO implement
  return true;
}

void Interface::WriteTo(std::string *result) const {
  result->append("intf.");
  result->append(std::to_string(reinterpret_cast<uintptr_t>(this)));
}

ir::Results Interface::PrepareArgument(Type const *t, ir::Results const &val,
                                       Context *ctx) const {
  UNREACHABLE();
}

void Interface::EmitCopyAssign(Type const *from_type, ir::Results const &from,
                               ir::RegisterOr<ir::Addr> to,
                               Context *ctx) const {
  UNREACHABLE();
}

void Interface::EmitMoveAssign(Type const *from_type, ir::Results const &from,
                               ir::RegisterOr<ir::Addr> to,
                               Context *ctx) const {
  UNREACHABLE();
}

void Interface::EmitRepr(ir::Results const &id_val, Context *ctx) const {
  UNREACHABLE();
}

core::Bytes Interface::bytes(core::Arch const &a) const {
  return core::Host().ptr_bytes;
}

core::Alignment Interface::alignment(core::Arch const &a) const {
  return core::Host().ptr_alignment;
}

bool Interface::ReinterpretAs(Type const *t) const { return t == this; }

}  // namespace type
