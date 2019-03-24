#include "type/opaque.h"

#include "base/debug.h"
#include "ir/val.h"

namespace type {
void Opaque::WriteTo(std::string *result) const { result->append("<opaque>"); }

void Opaque::EmitCopyAssign(const Type *from_type, ir::Results const &from,
                            ir::RegisterOr<ir::Addr> to, Context *ctx) const {
  UNREACHABLE();
}

void Opaque::EmitMoveAssign(const Type *from_type, ir::Results const &from,
                            ir::RegisterOr<ir::Addr> to, Context *ctx) const {
  UNREACHABLE();
}

void Opaque::EmitInit(ir::Register reg, Context *ctx) const { UNREACHABLE(); }

void Opaque::EmitDestroy(ir::Register reg, Context *ctx) const {
  UNREACHABLE();
}

ir::Results Opaque::PrepareArgument(const Type *t, const ir::Results &val,
                                    Context *ctx) const {
  UNREACHABLE();
}

void Opaque::EmitRepr(ir::Results const &id_val, Context *ctx) const {
  UNREACHABLE();
}

void Opaque::defining_modules(
    absl::flat_hash_set<::Module const *> *modules) const {
  modules->insert(mod_);
}

layout::Bytes Opaque::bytes(layout::Arch const &a) const {
  NOT_YET("figure out what to do here");
}

layout::Alignment Opaque::alignment(layout::Arch const &a) const {
  NOT_YET("figure out what to do here");
}

Cmp Opaque::Comparator() const { UNREACHABLE(); }

// TODO this is interesting. You could maybe be allowed to reinterpret any type
// as opaque, but we'd quickly run into problems. For example, there would be no
// checking that it's actually the correct opaque type. I.e., I could
// reinterpret int16 and int32 as the same opaque type. If a library actually
// knew what that opaque type was it would use them wrong. You should only be
// allowed to do this cast if you own the opaque type? Anyway, this doesn't go
// here... Opaque::ReinterpretAs should definitely just be cheking t == this,
// but other types maybe should be allowed to reinterpret to an opaque type.
bool Opaque::ReinterpretAs(Type const *t) const { return t == this; }

}  // namespace type
