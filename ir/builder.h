#ifndef ICARUS_IR_BUILDER_H
#define ICARUS_IR_BUILDER_H

#include <vector>

#include "absl/types/span.h"
#include "ast/overload_set.h"
#include "base/debug.h"
#include "base/meta.h"
#include "base/scope.h"
#include "base/tag.h"
#include "base/untyped_buffer.h"
#include "ir/blocks/basic.h"
#include "ir/blocks/group.h"
#include "ir/instruction/core.h"
#include "ir/instruction/instructions.h"
#include "ir/local_block_interpretation.h"
#include "ir/out_params.h"
#include "ir/struct_field.h"
#include "ir/value/addr.h"
#include "ir/value/reg.h"
#include "ir/value/scope.h"
#include "type/enum.h"
#include "type/jump.h"
#include "type/typed_value.h"
#include "type/util.h"

namespace ir {

struct Builder {
  BasicBlock* AddBlock();
  BasicBlock* AddBlock(BasicBlock const& to_copy);

  ir::OutParams OutParams(absl::Span<type::Type const* const> types);
  ir::OutParams OutParamsCopyInit(
      absl::Span<type::Type const* const> types,
      absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to);
  ir::OutParams OutParamsMoveInit(
      absl::Span<type::Type const* const> types,
      absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to);
  ir::OutParams OutParamsAssign(
      absl::Span<type::Type const* const> types,
      absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to);

  template <typename KeyType, typename ValueType>
  absl::flat_hash_map<KeyType, ir::BasicBlock*> AddBlocks(
      absl::flat_hash_map<KeyType, ValueType> const& table) {
    absl::flat_hash_map<KeyType, ir::BasicBlock*> result;
    for (auto const& [key, val] : table) { result.emplace(key, AddBlock()); }
    return result;
  }

  absl::flat_hash_map<ast::Expression const*, ir::BasicBlock*> AddBlocks(
      ast::OverloadSet const& os) {
    absl::flat_hash_map<ast::Expression const*, ir::BasicBlock*> result;
    for (auto const* overload : os.members()) {
      result.emplace(overload, AddBlock());
    }
    return result;
  }

  internal::BlockGroupBase*& CurrentGroup() { return current_.group_; }
  BasicBlock*& CurrentBlock() { return current_.block_; }

  template <typename T>
  struct reduced_type {
    using type = T;
  };
  template <typename T>
  struct reduced_type<RegOr<T>> {
    using type = T;
  };
  template <typename Tag>
  struct reduced_type<base::Tagged<Tag, Reg>> {
    using type = Tag;
  };

  template <typename T>
  using reduced_type_t = typename reduced_type<T>::type;

  // INSTRUCTIONS

  template <typename Lhs, typename Rhs>
  RegOr<reduced_type_t<Lhs>> Add(Lhs const& lhs, Rhs const& rhs) {
    AddInstruction<reduced_type_t<Lhs>> inst{.lhs = lhs, .rhs = rhs};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  template <typename Lhs, typename Rhs>
  RegOr<reduced_type_t<Lhs>> Sub(Lhs const& lhs, Rhs const& rhs) {
    SubInstruction<reduced_type_t<Lhs>> inst{.lhs = lhs, .rhs = rhs};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  template <typename Lhs, typename Rhs>
  RegOr<reduced_type_t<Lhs>> Mul(Lhs const& lhs, Rhs const& rhs) {
    MulInstruction<reduced_type_t<Lhs>> inst{.lhs = lhs, .rhs = rhs};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  template <typename Lhs, typename Rhs>
  RegOr<reduced_type_t<Lhs>> Div(Lhs const& lhs, Rhs const& rhs) {
    DivInstruction<reduced_type_t<Lhs>> inst{.lhs = lhs, .rhs = rhs};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  template <typename Lhs, typename Rhs>
  RegOr<reduced_type_t<Lhs>> Mod(Lhs const& lhs, Rhs const& rhs) {
    ModInstruction<reduced_type_t<Lhs>> inst{.lhs = lhs, .rhs = rhs};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  template <typename Lhs, typename Rhs>
  RegOr<bool> Lt(Lhs const& lhs, Rhs const& rhs) {
    using type = reduced_type_t<Lhs>;
    if constexpr (base::meta<Lhs>.template is_a<ir::RegOr>() and
                  base::meta<Rhs>.template is_a<ir::RegOr>()) {
      if (not lhs.is_reg() and not rhs.is_reg()) {
        return LtInstruction<type>::Apply(lhs.value(), rhs.value());
      }

      LtInstruction<reduced_type_t<Lhs>> inst{.lhs = lhs, .rhs = rhs};
      auto result = inst.result = CurrentGroup()->Reserve();
      CurrentBlock()->Append(std::move(inst));
      return result;
    } else {
      return Lt(RegOr<type>(lhs), RegOr<type>(rhs));
    }
  }

  template <typename Lhs, typename Rhs>
  RegOr<bool> Gt(Lhs const& lhs, Rhs const& rhs) {
    return Lt(rhs, lhs);
  }

  template <typename Lhs, typename Rhs>
  RegOr<bool> Le(Lhs const& lhs, Rhs const& rhs) {
    using type = reduced_type_t<Lhs>;
    if constexpr (base::meta<Lhs>.template is_a<ir::RegOr>() and
                  base::meta<Rhs>.template is_a<ir::RegOr>()) {
      if (not lhs.is_reg() and not rhs.is_reg()) {
        return LeInstruction<type>::Apply(lhs.value(), rhs.value());
      }

      LeInstruction<type> inst{.lhs = lhs, .rhs = rhs};
      auto result = inst.result = CurrentGroup()->Reserve();
      CurrentBlock()->Append(std::move(inst));
      return result;
    } else {
      return Le(RegOr<type>(lhs), RegOr<type>(rhs));
    }
  }

  template <typename Lhs, typename Rhs>
  RegOr<bool> Ge(Lhs const& lhs, Rhs const& rhs) {
    return Le(rhs, lhs);
  }

  // Flags operators
  RegOr<FlagsVal> XorFlags(RegOr<FlagsVal> const& lhs,
                           RegOr<FlagsVal> const& rhs) {
    using InstrT = XorFlagsInstruction;
    if (not lhs.is_reg() and not rhs.is_reg()) {
      return InstrT::Apply(lhs.value(), rhs.value());
    }
    InstrT inst{.lhs = lhs, .rhs = rhs};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  RegOr<FlagsVal> AndFlags(RegOr<FlagsVal> const& lhs,
                           RegOr<FlagsVal> const& rhs) {
    using InstrT = AndFlagsInstruction;
    if (not lhs.is_reg() and not rhs.is_reg()) {
      return InstrT::Apply(lhs.value(), rhs.value());
    }
    InstrT inst{.lhs = lhs, .rhs = rhs};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  RegOr<FlagsVal> OrFlags(RegOr<FlagsVal> const& lhs,
                          RegOr<FlagsVal> const& rhs) {
    using InstrT = OrFlagsInstruction;
    if (not lhs.is_reg() and not rhs.is_reg()) {
      return InstrT::Apply(lhs.value(), rhs.value());
    }
    InstrT inst{.lhs = lhs, .rhs = rhs};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  // Comparison
  template <typename Lhs, typename Rhs>
  RegOr<bool> Eq(Lhs const& lhs, Rhs const& rhs) {
    using type = reduced_type_t<Lhs>;
    if constexpr (base::meta<type> == base::meta<bool>) {
      return EqBool(lhs, rhs);
    } else if constexpr (base::meta<Lhs>.template is_a<ir::RegOr>() and
                         base::meta<Rhs>.template is_a<ir::RegOr>()) {
      if (not lhs.is_reg() and not rhs.is_reg()) {
        return EqInstruction<type>::Apply(lhs.value(), rhs.value());
      }
      EqInstruction<type> inst{.lhs = lhs, .rhs = rhs};
      auto result = inst.result = CurrentGroup()->Reserve();
      CurrentBlock()->Append(std::move(inst));
      return result;
    } else {
      return Eq(RegOr<type>(lhs), RegOr<type>(rhs));
    }
  }

  RegOr<bool> Eq(type::Type const* common_type, ir::Value const& lhs_val,
                 ir::Value const& rhs_val) {
    return type::ApplyTypes<bool, int8_t, int16_t, int32_t, int64_t, uint8_t,
                            uint16_t, uint32_t, uint64_t, float, double,
                            ir::EnumVal, ir::FlagsVal>(
        common_type, [&](auto tag) {
          using T = typename decltype(tag)::type;
          return Eq(lhs_val.get<RegOr<T>>(), rhs_val.get<RegOr<T>>());
        });
  }

  template <typename Lhs, typename Rhs>
  RegOr<bool> Ne(Lhs const& lhs, Rhs const& rhs) {
    using type = reduced_type_t<Lhs>;
    if constexpr (std::is_same_v<type, bool>) {
      return NeBool(lhs, rhs);
    } else if constexpr (base::meta<Lhs>.template is_a<ir::RegOr>() and
                         base::meta<Rhs>.template is_a<ir::RegOr>()) {
      if (not lhs.is_reg() and not rhs.is_reg()) {
        return NeInstruction<type>::Apply(lhs.value(), rhs.value());
      }
      NeInstruction<type> inst{.lhs = lhs, .rhs = rhs};
      auto result = inst.result = CurrentGroup()->Reserve();
      CurrentBlock()->Append(std::move(inst));
      return result;
    } else {
      return Ne(RegOr<type>(lhs), RegOr<type>(rhs));
    }
  }

  template <typename T>
  RegOr<T> Neg(RegOr<T> const& val) {
    using InstrT = NegInstruction<T>;
    if (not val.is_reg()) { return InstrT::Apply(val.value()); }
    NegInstruction<reduced_type_t<T>> inst{.operand = val};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  // Note: Even though this must return a more specific type (BufferPointer
  // instead of Type), we use Type to ensure that if this gets routed into an
  // ir::Value, it will be tagged correctly.
  RegOr<type::Type const*> BufPtr(RegOr<type::Type const*> const& val) {
    using InstrT = BufPtrInstruction;
    if (not val.is_reg()) { return InstrT::Apply(val.value()); }
    InstrT inst{.operand = val};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  // Note: Even though this must return a more specific type (Pointer instead of
  // Type), we use Type to ensure that if this gets routed into an ir::Value, it
  // will be tagged correctly.
  RegOr<type::Type const*> Ptr(RegOr<type::Type const*> const& val) {
    using InstrT = PtrInstruction;
    if (not val.is_reg()) { return InstrT::Apply(val.value()); }
    InstrT inst{.operand = val};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  RegOr<bool> Not(RegOr<bool> const& val) {
    using InstrT = NotInstruction;
    if (not val.is_reg()) { return InstrT::Apply(val.value()); }
    InstrT inst{.operand = val};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  RegOr<type::Type const*> Tup(std::vector<RegOr<type::Type const*>> types) {
    // TODO constant-folding
    TupleInstruction inst{.values = std::move(types)};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  template <typename ToType>
  RegOr<ToType> CastTo(type::Typed<ir::Value> v) {
    if constexpr (base::meta<ToType> != base::meta<ir::EnumVal> and
                  base::meta<ToType> != base::meta<ir::FlagsVal>) {
      if (v.type() == type::Get<ToType>()) { return v->get<RegOr<ToType>>(); }
    }

    if (v.type() == type::Int8) {
      return Cast<int8_t, ToType>(v->get<RegOr<int8_t>>());
    } else if (v.type() == type::Nat8) {
      return Cast<uint8_t, ToType>(v->get<RegOr<uint8_t>>());
    } else if (v.type() == type::Int16) {
      return Cast<int16_t, ToType>(v->get<RegOr<int16_t>>());
    } else if (v.type() == type::Nat16) {
      return Cast<uint16_t, ToType>(v->get<RegOr<uint16_t>>());
    } else if (v.type() == type::Int32) {
      return Cast<int32_t, ToType>(v->get<RegOr<int32_t>>());
    } else if (v.type() == type::Nat32) {
      return Cast<uint32_t, ToType>(v->get<RegOr<uint32_t>>());
    } else if (v.type() == type::Int64) {
      return Cast<int64_t, ToType>(v->get<RegOr<int64_t>>());
    } else if (v.type() == type::Nat64) {
      return Cast<uint64_t, ToType>(v->get<RegOr<uint64_t>>());
    } else if (v.type() == type::Float32) {
      return Cast<float, ToType>(v->get<RegOr<float>>());
    } else if (v.type() == type::Float64) {
      return Cast<double, ToType>(v->get<RegOr<double>>());
    } else {
      UNREACHABLE();
    }
  }

  // Phi instruction. Takes a span of basic blocks and a span of (registers or)
  // values. As a precondition, the number of blocks must be equal to the number
  // of values. This instruction evaluates to the value `values[i]` if the
  // previous block was `blocks[i]`.
  //
  // In the first overload, the resulting value is assigned to `r`. In the
  // second overload, a register is constructed to represent the value.
  template <typename T>
  void Phi(Reg r, std::vector<BasicBlock const*> blocks,
           std::vector<RegOr<T>> values) {
    ASSERT(blocks.size() == values.size());
    PhiInstruction<T> inst(std::move(blocks), std::move(values));
    inst.result = r;
  }

  template <typename T>
  RegOr<T> Phi(std::vector<BasicBlock const*> blocks,
               std::vector<RegOr<T>> values) {
    if (values.size() == 1u) { return values[0]; }
    PhiInstruction<T> inst(std::move(blocks), std::move(values));
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  // Usually it is sufficient to determine all the inputs to a phi instruction
  // upfront, but sometimes it is useful to construct a phi instruction without
  // having set its inputs.
  //
  // TODO: Right now we are relying on the fact that Inst stores values on the
  // heap, but this may not always be the case.
  template <typename T>
  PhiInstruction<T>* PhiInst() {
    PhiInstruction<T> inst;
    inst.result = CurrentGroup()->Reserve();
    return CurrentBlock()
        ->Append(std::move(inst))
        .template if_as<PhiInstruction<T>>();
  }

  template <typename F>
  void OnEachArrayElement(type::Array const* t, Reg array_reg, F fn) {
    auto* data_ptr_type = type::Ptr(t->data_type());

    auto ptr = PtrIncr(array_reg, 0, type::Ptr(data_ptr_type));
    auto end_ptr =
        PtrIncr(ptr, static_cast<int32_t>(t->length()), data_ptr_type);

    auto* start_block = CurrentBlock();
    auto* loop_body   = AddBlock();
    auto* land_block  = AddBlock();
    auto* cond_block  = AddBlock();

    UncondJump(cond_block);

    CurrentBlock() = cond_block;
    auto* phi      = PhiInst<Addr>();
    CondJump(Eq(RegOr<Addr>(phi->result), end_ptr), land_block, loop_body);

    CurrentBlock() = loop_body;
    fn(phi->result);
    Reg next = PtrIncr(phi->result, 1, data_ptr_type);
    UncondJump(cond_block);

    phi->add(start_block, ptr);
    phi->add(CurrentBlock(), next);

    CurrentBlock() = land_block;
  }

  Reg PtrFix(Reg r, type::Type const* desired_type) {
    // TODO must this be a register if it's loaded?
    return desired_type->is_big() ? r : Load(r, desired_type).get<Reg>();
  }

  template <typename T>
  RegOr<T> Load(RegOr<Addr> addr) {
    auto& blk = *CurrentBlock();

    // TODO Just take a Reg. RegOr<Addr> is overkill and not possible because
    // constants don't have addresses.
    Value& cache_results = blk.load_store_cache().slot<T>(addr);
    if (not cache_results.empty()) {
      // TODO may not be Reg. could be anything of the right type.
      return cache_results.get<RegOr<T>>();
    }

    LoadInstruction inst{.num_bytes = core::Bytes::Get<T>().value(),
                         .addr      = addr};
    auto result = inst.result = CurrentGroup()->Reserve();

    cache_results = Value(result);

    blk.Append(std::move(inst));
    return result;
  }

  Value Load(RegOr<Addr> r, type::Type const* t) {
    using base::stringify;
    LOG("Load", "Calling Load(%s, %s)", r, t->to_string());
    if (t->is<type::Function>()) { return Value(Load<Fn>(r)); }
    return type::ApplyTypes<bool, int8_t, int16_t, int32_t, int64_t, uint8_t,
                            uint16_t, uint32_t, uint64_t, float, double,
                            type::Type const*, EnumVal, FlagsVal, Addr, String,
                            Fn>(t, [&](auto tag) {
      using T = typename decltype(tag)::type;
      return Value(Load<T>(r));
    });
  }

  template <typename T>
  void Store(T r, RegOr<Addr> addr) {
    if constexpr (base::meta<T>.template is_a<ir::RegOr>()) {
      auto& blk = *CurrentBlock();
      blk.load_store_cache().clear<typename T::type>();
      blk.Append(
          StoreInstruction<typename T::type>{.value = r, .location = addr});
    } else {
      Store(RegOr<T>(r), addr);
    }
  }

  Reg GetRet(uint16_t n, type::Type const* t) {
    GetReturnInstruction inst{.index = n};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  Reg Index(type::Pointer const* t, Reg array_ptr, RegOr<int64_t> offset) {
    return PtrIncr(array_ptr, offset,
                   type::Ptr(t->pointee()->as<type::Array>().data_type()));
  }

  // Emits a function-call instruction, calling `fn` of type `f` with the given
  // `arguments` and output parameters. If output parameters are not present,
  // the function must return nothing.
  void Call(RegOr<Fn> const& fn, type::Function const* f,
            std::vector<Value> args, ir::OutParams outs);

  // Jump instructions must be the last instruction in a basic block. They
  // handle control-flow, indicating which basic block control should be
  // transferred to next.
  //
  // `UncondJump`: Transfers control to `block`.
  // `CondJump`:   Transfers control to one of two blocks depending on a
  //               run-time boolean value.
  // `ReturnJump`: Transfers control back to the calling function.
  //
  // `ChooseJump`: Transfers control to the appropriate block-handler. Note that
  //               this is highly specific to the current scope-definine
  //               language constructs which are likely to change.
  void UncondJump(BasicBlock* block);
  void CondJump(RegOr<bool> cond, BasicBlock* true_block,
                BasicBlock* false_block);
  void ReturnJump();
  // TODO: Probably better to have a data structure for this.
  void ChooseJump(std::vector<std::string_view> names,
                  std::vector<BasicBlock*> blocks,
                  std::vector<core::FnArgs<type::Typed<Value>>> args);

  template <bool B>
  BasicBlock* EarlyExitOn(BasicBlock* exit_block, RegOr<bool> cond) {
    auto* continue_block = AddBlock();
    if constexpr (B) {
      CondJump(cond, exit_block, continue_block);
    } else {
      CondJump(cond, continue_block, exit_block);
    }
    return continue_block;
  }

  // Special members function instructions. Calling these typically calls
  // builtin functions (or, in the case of primitive types, do nothing).
  //
  // TODO: Use Typed<Reg>
  void Init(type::Type const* t, Reg r);
  void Destroy(type::Type const* t, Reg r);
  void Move(type::Type const* t, Reg from, RegOr<Addr> to);
  void Copy(type::Type const* t, Reg from, RegOr<Addr> to);

  // Data structure access commands. For structs and tuples, `Fields` takes an
  // address of the data structure and returns the address of the particular
  // field requested. For variants, `VariantType` computes the location where
  // the type is stored and `VariantValue` accesses the location where the
  // value is stored.
  //
  // TODO: Long-term, variant will probably not be implemented this way.
  // Ideally, something like `*int64 | *int32` will only use 8 bytes because
  // we'll be able to see that the pointers are aligned and we have spare bits.
  // This means variant isn't the lowest level API, but rather some mechanism
  // by which you can overlay types.
  type::Typed<Reg> Field(RegOr<Addr> r, type::Struct const* t, int64_t n);
  type::Typed<Reg> Field(RegOr<Addr> r, type::Tuple const* t, int64_t n);

  Reg PtrIncr(RegOr<Addr> ptr, RegOr<int64_t> inc, type::Pointer const* t);
  // TODO should this be unsigned?
  RegOr<int64_t> ByteViewLength(RegOr<ir::String> val);
  RegOr<Addr> ByteViewData(RegOr<ir::String> val);

  // Type construction commands

  // Note: Even though this must return a more specific type (Function instead
  // of Type), we use Type to ensure that if this gets routed into an ir::Value,
  // it will be tagged correctly.
  RegOr<type::Type const*> Arrow(
      std::vector<RegOr<type::Type const*>> const& ins,
      std::vector<RegOr<type::Type const*>> const& outs);

  RegOr<type::Type const*> Array(RegOr<ArrayInstruction::length_t> len,
                                 RegOr<type::Type const*> data_type);

  Reg OpaqueType(module::BasicModule const* mod);

  Reg Struct(type::Struct* s, std::vector<StructField> fields,
             std::optional<ir::Fn> assign, std::optional<ir::Fn> dtor);

  Reg Enum(type::Enum* e, std::vector<std::string_view> names,
           absl::flat_hash_map<uint64_t, RegOr<uint64_t>> specified_values);
  Reg Flags(type::Flags* f, std::vector<std::string_view> names,
            absl::flat_hash_map<uint64_t, RegOr<uint64_t>> specified_values);

  type::Typed<Reg> LoadSymbol(String name, type::Type const* type);

  // Low-level size/alignment commands
  Reg Align(RegOr<type::Type const*> r);
  Reg Bytes(RegOr<type::Type const*> r);

  Reg Alloca(type::Type const* t);
  Reg TmpAlloca(type::Type const* t);

  Reg MakeBlock(Block block, std::vector<RegOr<Fn>> befores,
                std::vector<RegOr<Jump>> afters);
  Reg MakeScope(ir::Scope scope, std::vector<RegOr<Jump>> inits,
                std::vector<RegOr<Fn>> dones,
                absl::flat_hash_map<std::string_view, Block> blocks);

  void DebugIr() { CurrentBlock()->Append(DebugIrInstruction{}); }

  LocalBlockInterpretation MakeLocalBlockInterpretation(
      ast::ScopeNode const*, BasicBlock* starting_block,
      BasicBlock* landing_block);

  // Apply the callable to each temporary in reverse order, and clear the list
  // of temporaries.
  template <typename Fn>
  void FinishTemporariesWith(Fn&& fn) {
    for (auto iter = current_.temporaries_to_destroy_.rbegin();
         iter != current_.temporaries_to_destroy_.rend(); ++iter) {
      fn(*iter);
    }
    current_.temporaries_to_destroy_.clear();
  }

  enum class BlockTerminationState {
    kMoreStatements,  // Not at the end of the block yet
    kNoTerminator,    // Block complete; no `return` or `<<`
    kReturn,          // Block completed with `return`
    kLabeledYield,    // Block completed with `#.my_label << `
    kYield,           // Block completed with `<<`
  };
  constexpr BlockTerminationState block_termination_state() const {
    return current_.block_termination_state_;
  }
  constexpr BlockTerminationState& block_termination_state() {
    return current_.block_termination_state_;
  }

  template <typename T>
  void SetRet(uint16_t n, T val) {
    if constexpr (base::meta<T>.template is_a<ir::RegOr>()) {
      SetReturnInstruction<typename T::type> inst{.index = n, .value = val};
      CurrentBlock()->Append(std::move(inst));
    } else if constexpr (base::IsTaggedV<T>) {
      static_assert(base::meta<typename T::base_type> == base::meta<Reg>);
      SetRet(n, RegOr<typename T::tag_type>(val));
    } else {
      SetRet(n, RegOr<T>(val));
    }
  }

  void SetRet(uint16_t n, type::Typed<Value> const& r) {
    ASSERT(r.type()->is_big() == false);
    type::Apply(r.type(), [&](auto tag) {
      using T = typename decltype(tag)::type;
      SetRet(n, r->get<RegOr<T>>());
    });
  }

 private:
  template <typename FromType, typename ToType>
  RegOr<ToType> Cast(RegOr<FromType> r) {
    if (r.is_reg()) {
      CastInstruction<ToType, FromType> inst{.value = r.reg()};
      auto result = inst.result = CurrentGroup()->Reserve();
      CurrentBlock()->Append(std::move(inst));
      return result;
    } else {
      return static_cast<ToType>(r.value());
    }
  }

  RegOr<bool> EqBool(RegOr<bool> const& lhs, RegOr<bool> const& rhs) {
    if (not lhs.is_reg()) { return lhs.value() ? rhs : Not(rhs); }
    if (not rhs.is_reg()) { return rhs.value() ? lhs : Not(lhs); }
    EqInstruction<bool> inst{.lhs = lhs, .rhs = rhs};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  RegOr<bool> NeBool(RegOr<bool> const& lhs, RegOr<bool> const& rhs) {
    if (not lhs.is_reg()) { return lhs.value() ? Not(rhs) : rhs; }
    if (not rhs.is_reg()) { return rhs.value() ? Not(lhs) : lhs; }
    NeInstruction<bool> inst{.lhs = lhs, .rhs = rhs};
    auto result = inst.result = CurrentGroup()->Reserve();
    CurrentBlock()->Append(std::move(inst));
    return result;
  }

  friend struct SetCurrent;
  friend struct SetTemporaries;

  struct State {
    internal::BlockGroupBase* group_ = nullptr;
    BasicBlock* block_;

    // Temporaries need to be destroyed at the end of each statement.
    // This is a pointer to a buffer where temporary allocations can register
    // themselves for deletion.
    std::vector<type::Typed<Reg>> temporaries_to_destroy_;
    BlockTerminationState block_termination_state_ =
        BlockTerminationState::kMoreStatements;
  } current_;
};

Builder& GetBuilder();

struct SetCurrent : public base::UseWithScope {
  explicit SetCurrent(internal::BlockGroupBase* fn, Builder* builder = nullptr);
  explicit SetCurrent(NativeFn fn) : SetCurrent(fn.get()) {}
  ~SetCurrent();

 private:
  Builder* builder_;
  internal::BlockGroupBase* old_group_;
  BasicBlock* old_block_;
  Builder::BlockTerminationState old_termination_state_;
};

struct SetTemporaries : public base::UseWithScope {
  SetTemporaries(Builder& bldr) : bldr_(bldr) {
    old_temporaries_ = std::exchange(bldr_.current_.temporaries_to_destroy_,
                                     std::vector<type::Typed<Reg>>{});
  }
  ~SetTemporaries() {}

 private:
  std::vector<type::Typed<Reg>> old_temporaries_;
  Builder& bldr_;
};

template <typename T>
Reg MakeReg(T t) {
  static_assert(not std::is_same_v<T, Reg>);
  if constexpr (base::meta<T>.template is_a<ir::RegOr>()) {
    RegisterInstruction<typename T::type> inst{.operand = t};
    auto result = inst.result = GetBuilder().CurrentGroup()->Reserve();
    GetBuilder().CurrentBlock()->Append(std::move(inst));
    return result;
  } else {
    return MakeReg(RegOr<T>{t});
  }
}

}  // namespace ir

#endif  // ICARUS_IR_BUILDER_H
