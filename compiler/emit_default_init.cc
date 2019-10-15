#include "absl/random/random.h"
#include "ast/ast.h"
#include "base/permutation.h"
#include "compiler/compiler.h"
#include "ir/builder.h"
#include "ir/cmd/misc.h"
#include "ir/cmd/store.h"
#include "ir/compiled_fn.h"
#include "ir/components.h"
#include "ir/results.h"
#include "type/primitive.h"

namespace compiler {

void Compiler::EmitDefaultInit(type::Array const *t, ir::Reg reg) {
  t->init_func_.init([=]() {
    // TODO special function?
    auto *fn = AddFunc(
        type::Func({type::Ptr(t)}, {}),
        core::FnParams(core::Param{
            "", type::Typed<ast::Expression const *>{nullptr, type::Ptr(t)}}));
    ir::OnEachArrayElement(
        t, fn, [=](ir::Reg r) { t->data_type->EmitDefaultInit(this, r); });
    return fn;
  });

  ir::Init(t, reg);
}

void Compiler::EmitDefaultInit(type::Flags const *t, ir::Reg reg) {
  ir::Store(ir::FlagsVal{0}, reg);
}

void Compiler::EmitDefaultInit(type::Pointer const *t, ir::Reg reg) {
  ir::Store(ir::Addr::Null(), reg);
}

void Compiler::EmitDefaultInit(type::Primitive const *t, ir::Reg reg) {
  switch (t->type_) {
    case type::BasicType::Type_: ir::Store(type::Void(), reg); break;
    case type::BasicType::NullPtr: UNREACHABLE();
    case type::BasicType::EmptyArray: UNREACHABLE();
    case type::BasicType::Bool: ir::Store(false, reg); break;
    case type::BasicType::Int8: ir::Store(static_cast<int8_t>(0), reg); break;
    case type::BasicType::Int16: ir::Store(static_cast<int16_t>(0), reg); break;
    case type::BasicType::Int32: ir::Store(static_cast<int32_t>(0), reg); break;
    case type::BasicType::Int64: ir::Store(static_cast<int64_t>(0), reg); break;
    case type::BasicType::Nat8: ir::Store(static_cast<uint8_t>(0), reg); break;
    case type::BasicType::Nat16:
      ir::Store(static_cast<uint16_t>(0), reg);
      break;
    case type::BasicType::Nat32:
      ir::Store(static_cast<uint32_t>(0), reg);
      break;
    case type::BasicType::Nat64:
      ir::Store(static_cast<uint64_t>(0), reg);
      break;
    case type::BasicType::Float32: ir::Store(0.0f, reg); break;
    case type::BasicType::Float64: ir::Store(0.0, reg); break;
    default: UNREACHABLE();
  }
}

void Compiler::EmitDefaultInit(type::Struct const *t, ir::Reg reg) {
  ir::Init(t, reg);
}

void Compiler::EmitDefaultInit(type::Tuple const *t, ir::Reg reg) {
  t->init_func_.init([=]() {
    auto *fn = AddFunc(
        type::Func({type::Ptr(t)}, {}),
        core::FnParams(core::Param{
            "", type::Typed<ast::Expression const *>{nullptr, type::Ptr(t)}}));

    ICARUS_SCOPE(ir::SetCurrentFunc(fn)) {
      builder().CurrentBlock() = builder().function()->entry();
      auto var                 = ir::Reg::Arg(0);

      for (size_t i :
           base::make_random_permutation(absl::BitGen{}, t->entries_.size())) {
        t->entries_.at(i)->EmitDefaultInit(this, ir::Field(var, t, i).get());
      }

      ir::ReturnJump();
    }
    return fn;
  });

  ir::Init(t, reg);
}

}  // namespace compiler
