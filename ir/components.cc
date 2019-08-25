#include "ir/components.h"

#include "misc/context.h"

namespace ir {
RegOr<bool> EmitEq(type::Type const *lhs_type, ir::Results const &lhs_val,
                        type::Type const *rhs_type,
                        ir::Results const &rhs_val) {
  // You may already assume that there exists a valid comparison between these
  // two types.
  if (lhs_type != rhs_type) { NOT_YET(); }

  return type::ApplyTypes<bool, int8_t, int16_t, int32_t, int64_t, uint8_t,
                          uint16_t, uint32_t, uint64_t, float, double>(
      lhs_type, [&](auto type_holder) {
        using T = typename decltype(type_holder)::type;
        return ir::Eq(lhs_val.get<T>(0), rhs_val.get<T>(0));
      });
}

TypedRegister<Addr> Index(type::Pointer const *t, Reg array_ptr,
                          RegOr<int64_t> offset) {
  // TODO this works but generates worse ir (both here and in llvm). It's worth
  // figuring out how to do this better. Is this still true without
  // variable-length arrays?
  return PtrIncr(array_ptr, offset,
                 type::Ptr(t->pointee->as<type::Array>().data_type));
}

TypedRegister<Addr> Alloca(type::Type const *t) {
  return CompiledFn::Current->Alloca(t);
}

TypedRegister<Addr> TmpAlloca(type::Type const *t, Context *ctx) {
  auto reg = Alloca(t);
  ctx->temporaries_to_destroy_->emplace_back(reg, t);
  return reg;
}

}  // namespace ir
