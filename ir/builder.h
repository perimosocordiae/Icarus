#ifndef ICARUS_IR_BUILDER_H
#define ICARUS_IR_BUILDER_H

#include <vector>

#include "absl/types/span.h"
#include "base/debug.h"
#include "base/scope.h"
#include "base/tag.h"
#include "base/untyped_buffer.h"
#include "ir/addr.h"
#include "ir/basic_block.h"
#include "ir/block_group.h"
#include "ir/instructions.h"
#include "ir/local_block_interpretation.h"
#include "ir/out_params.h"
#include "ir/reg.h"
#include "ir/struct_field.h"
#include "type/jump.h"
#include "type/typed_value.h"
#include "type/util.h"

namespace ir {

struct Builder {
  BasicBlock* AddBlock();
  BasicBlock* AddBlock(BasicBlock const& to_copy);

  ir::OutParams OutParams(absl::Span<type::Type const* const> types);

  template <typename KeyType, typename ValueType>
  absl::flat_hash_map<KeyType, ir::BasicBlock*> AddBlocks(
      absl::flat_hash_map<KeyType, ValueType> const& table) {
    absl::flat_hash_map<KeyType, ir::BasicBlock*> result;
    for (auto const& [key, val] : table) { result.emplace(key, AddBlock()); }
    return result;
  }

  internal::BlockGroup*& CurrentGroup() { return current_.group_; }
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
    auto inst = std::make_unique<AddInstruction<reduced_type_t<Lhs>>>(lhs, rhs);
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  template <typename Lhs, typename Rhs>
  RegOr<reduced_type_t<Lhs>> Sub(Lhs const& lhs, Rhs const& rhs) {
    auto inst = std::make_unique<SubInstruction<reduced_type_t<Lhs>>>(lhs, rhs);
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  template <typename Lhs, typename Rhs>
  RegOr<reduced_type_t<Lhs>> Mul(Lhs const& lhs, Rhs const& rhs) {
    auto inst = std::make_unique<MulInstruction<reduced_type_t<Lhs>>>(lhs, rhs);
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  template <typename Lhs, typename Rhs>
  RegOr<reduced_type_t<Lhs>> Div(Lhs const& lhs, Rhs const& rhs) {
    auto inst = std::make_unique<DivInstruction<reduced_type_t<Lhs>>>(lhs, rhs);
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  template <typename Lhs, typename Rhs>
  RegOr<reduced_type_t<Lhs>> Mod(Lhs const& lhs, Rhs const& rhs) {
    auto inst = std::make_unique<ModInstruction<reduced_type_t<Lhs>>>(lhs, rhs);
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  template <typename Lhs, typename Rhs>
  RegOr<bool> Lt(Lhs const& lhs, Rhs const& rhs) {
    using type = reduced_type_t<Lhs>;
    if constexpr (IsRegOr<Lhs>::value and IsRegOr<Rhs>::value) {
      if (not lhs.is_reg() and not rhs.is_reg()) {
        return LtInstruction<type>::Apply(lhs.value(), rhs.value());
      }

      auto inst =
          std::make_unique<LtInstruction<reduced_type_t<Lhs>>>(lhs, rhs);
      auto result = inst->result = CurrentGroup()->Reserve();
      CurrentBlock()->instructions_.push_back(std::move(inst));
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
    if constexpr (IsRegOr<Lhs>::value and IsRegOr<Rhs>::value) {
      if (not lhs.is_reg() and not rhs.is_reg()) {
        return LeInstruction<type>::Apply(lhs.value(), rhs.value());
      }

      auto inst   = std::make_unique<LeInstruction<type>>(lhs, rhs);
      auto result = inst->result = CurrentGroup()->Reserve();
      CurrentBlock()->instructions_.push_back(std::move(inst));
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
    auto inst   = std::make_unique<InstrT>(lhs, rhs);
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  RegOr<FlagsVal> AndFlags(RegOr<FlagsVal> const& lhs,
                           RegOr<FlagsVal> const& rhs) {
    using InstrT = AndFlagsInstruction;
    if (not lhs.is_reg() and not rhs.is_reg()) {
      return InstrT::Apply(lhs.value(), rhs.value());
    }
    auto inst   = std::make_unique<InstrT>(lhs, rhs);
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  RegOr<FlagsVal> OrFlags(RegOr<FlagsVal> const& lhs,
                          RegOr<FlagsVal> const& rhs) {
    using InstrT = OrFlagsInstruction;
    if (not lhs.is_reg() and not rhs.is_reg()) {
      return InstrT::Apply(lhs.value(), rhs.value());
    }
    auto inst   = std::make_unique<InstrT>(lhs, rhs);
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  // Comparison
  template <typename Lhs, typename Rhs>
  RegOr<bool> Eq(Lhs const& lhs, Rhs const& rhs) {
    using type = reduced_type_t<Lhs>;
    if constexpr (std::is_same_v<type, bool>) {
      return EqBool(lhs, rhs);
    } else if constexpr (IsRegOr<Lhs>::value and IsRegOr<Rhs>::value) {
      if (not lhs.is_reg() and not rhs.is_reg()) {
        return EqInstruction<type>::Apply(lhs.value(), rhs.value());
      }
      auto inst   = std::make_unique<EqInstruction<type>>(lhs, rhs);
      auto result = inst->result = CurrentGroup()->Reserve();
      CurrentBlock()->instructions_.push_back(std::move(inst));
      return result;
    } else {
      return Eq(RegOr<type>(lhs), RegOr<type>(rhs));
    }
  }

  template <typename Lhs, typename Rhs>
  RegOr<bool> Ne(Lhs const& lhs, Rhs const& rhs) {
    using type = reduced_type_t<Lhs>;
    if constexpr (std::is_same_v<type, bool>) {
      return NeBool(lhs, rhs);
    } else if constexpr (IsRegOr<Lhs>::value and IsRegOr<Rhs>::value) {
      if (not lhs.is_reg() and not rhs.is_reg()) {
        return NeInstruction<type>::Apply(lhs.value(), rhs.value());
      }
      auto inst   = std::make_unique<NeInstruction<type>>(lhs, rhs);
      auto result = inst->result = CurrentGroup()->Reserve();
      CurrentBlock()->instructions_.push_back(std::move(inst));
      return result;
    } else {
      return Ne(RegOr<type>(lhs), RegOr<type>(rhs));
    }
  }

  template <typename T>
  RegOr<T> Neg(RegOr<T> const& val) {
    using InstrT = NegInstruction<T>;
    if (not val.is_reg()) { return InstrT::Apply(val.value()); }
    auto inst   = std::make_unique<NegInstruction<reduced_type_t<T>>>(val);
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  RegOr<type::BufferPointer const*> BufPtr(
      RegOr<type::Type const*> const& val) {
    using InstrT = BufPtrInstruction;
    if (not val.is_reg()) { return InstrT::Apply(val.value()); }
    auto inst   = std::make_unique<InstrT>(val);
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  RegOr<type::Pointer const*> Ptr(RegOr<type::Type const*> const& val) {
    using InstrT = PtrInstruction;
    if (not val.is_reg()) { return InstrT::Apply(val.value()); }
    auto inst   = std::make_unique<InstrT>(val);
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  RegOr<bool> Not(RegOr<bool> const& val) {
    using InstrT = NotInstruction;
    if (not val.is_reg()) { return InstrT::Apply(val.value()); }
    auto inst   = std::make_unique<InstrT>(val);
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  RegOr<type::Type const*> Var(std::vector<RegOr<type::Type const*>> types) {
    // TODO constant-folding
    auto inst   = std::make_unique<VariantInstruction>(std::move(types));
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }
  RegOr<type::Type const*> Tup(std::vector<RegOr<type::Type const*>> types) {
    // TODO constant-folding
    auto inst   = std::make_unique<TupleInstruction>(std::move(types));
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
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
    auto inst    = std::make_unique<PhiInstruction<T>>(std::move(blocks),
                                                    std::move(values));
    inst->result = r;
  }

  template <typename T>
  RegOr<T> Phi(std::vector<BasicBlock const*> blocks,
               std::vector<RegOr<T>> values) {
    if (values.size() == 1u) { return values[0]; }
    auto inst   = std::make_unique<PhiInstruction<T>>(std::move(blocks),
                                                    std::move(values));
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  // Usually it is sufficient to determine all the inputs to a phi instruction
  // upfront, but sometimes it is useful to construct a phi instruction without
  // having set its inputs.
  template <typename T>
  PhiInstruction<T>* PhiInst() {
    auto inst = std::make_unique<PhiInstruction<T>>();

    inst->result   = CurrentGroup()->Reserve();
    auto* phi_inst = inst.get();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return phi_inst;
  }

  template <typename F>
  void OnEachArrayElement(type::Array const* t, Reg array_reg, F fn) {
    auto* data_ptr_type = type::Ptr(t->data_type);

    auto ptr     = PtrIncr(array_reg, 0, type::Ptr(data_ptr_type));
    auto end_ptr = PtrIncr(ptr, static_cast<int32_t>(t->len), data_ptr_type);

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

  // Emits a function-call instruction, calling `fn` of type `f` with the given
  // `arguments` and output parameters. If output parameters are not present,
  // the function must return nothing.
  void Call(RegOr<AnyFunc> const& fn, type::Function const* f,
            std::vector<Results> args, ir::OutParams outs);

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
                  std::vector<BasicBlock *> blocks,
                  std::vector<core::FnArgs<type::Typed<Results>>> args);

  // Special members function instructions. Calling these typically calls
  // builtin functions (or, in the case of primitive types, do nothing).
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
  Reg VariantType(RegOr<Addr> const& r);
  Reg VariantValue(type::Variant const* v, RegOr<Addr> const& r);

  base::Tagged<Addr, Reg> PtrIncr(RegOr<Addr> ptr, RegOr<int64_t> inc,
                                  type::Pointer const* t);
  // TODO should this be unsigned?
  RegOr<int64_t> ByteViewLength(RegOr<std::string_view> val);
  RegOr<Addr> ByteViewData(RegOr<std::string_view> val);

  // Type construction commands
  RegOr<type::Function const*> Arrow(
      std::vector<RegOr<type::Type const*>> const& ins,
      std::vector<RegOr<type::Type const*>> const& outs);

  RegOr<type::Type const*> Array(RegOr<ArrayInstruction::length_t> len,
                                 RegOr<type::Type const*> data_type);

  Reg OpaqueType(module::BasicModule const* mod);

  Reg Struct(ast::Scope const* scope, std::vector<StructField> fields);

  // TODO use scopes instead of modules.
  Reg Enum(module::BasicModule* mod, std::vector<std::string_view> names,
           absl::flat_hash_map<uint64_t, RegOr<uint64_t>> specified_values);
  Reg Flags(module::BasicModule* mod, std::vector<std::string_view> names,
            absl::flat_hash_map<uint64_t, RegOr<uint64_t>> specified_values);

  // Print commands
  template <typename T>
  void Print(T r) {
    if constexpr (IsRegOr<T>::value) {
      auto inst = std::make_unique<PrintInstruction<typename T::type>>(r);
      CurrentBlock()->instructions_.push_back(std::move(inst));
    } else if constexpr (std::is_same_v<T, char const*>) {
      Print(RegOr<std::string_view>(r));
    } else {
      Print(RegOr<T>(r));
    }
  }

  template <typename T,
            typename std::enable_if_t<std::is_same_v<T, EnumVal> or
                                      std::is_same_v<T, FlagsVal>>* = nullptr>
  void Print(RegOr<T> r, type::Type const* t) {
    if constexpr (std::is_same_v<T, EnumVal>) {
      auto inst =
          std::make_unique<PrintEnumInstruction>(r, &t->as<type::Enum>());
      CurrentBlock()->instructions_.push_back(std::move(inst));
    } else {
      auto inst =
          std::make_unique<PrintFlagsInstruction>(r, &t->as<type::Flags>());
      CurrentBlock()->instructions_.push_back(std::move(inst));
    }
  }

  type::Typed<Reg> LoadSymbol(std::string_view name, type::Type const* type);

  // Low-level size/alignment commands
  base::Tagged<core::Alignment, Reg> Align(RegOr<type::Type const*> r);
  base::Tagged<core::Bytes, Reg> Bytes(RegOr<type::Type const*> r);

  base::Tagged<Addr, Reg> Alloca(type::Type const* t);
  base::Tagged<Addr, Reg> TmpAlloca(type::Type const* t);

  Reg MakeBlock(ir::BlockDef* block_def, std::vector<RegOr<AnyFunc>> befores,
                std::vector<RegOr<Jump*>> afters);
  Reg MakeScope(ir::ScopeDef* scope_def, std::vector<RegOr<Jump*>> inits,
                std::vector<RegOr<AnyFunc>> dones,
                absl::flat_hash_map<std::string_view, BlockDef*> blocks);

  void DebugIr() {
    auto inst = std::make_unique<DebugIrInstruction>();
    CurrentBlock()->instructions_.push_back(std::move(inst));
  }

  LocalBlockInterpretation MakeLocalBlockInterpretation(ast::ScopeNode const*);

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

  constexpr bool more_stmts_allowed() const {
    return current_.more_stmts_allowed_;
  }
  constexpr void allow_more_stmts() { current_.more_stmts_allowed_ = true; }
  constexpr void disallow_more_stmts() { current_.more_stmts_allowed_ = false; }

  ICARUS_PRIVATE
  RegOr<bool> EqBool(RegOr<bool> const& lhs, RegOr<bool> const& rhs) {
    if (not lhs.is_reg()) { return lhs.value() ? rhs : Not(rhs); }
    if (not rhs.is_reg()) { return rhs.value() ? lhs : Not(lhs); }
    auto inst   = std::make_unique<EqInstruction<bool>>(lhs, rhs);
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  RegOr<bool> NeBool(RegOr<bool> const& lhs, RegOr<bool> const& rhs) {
    if (not lhs.is_reg()) { return lhs.value() ? Not(rhs) : rhs; }
    if (not rhs.is_reg()) { return rhs.value() ? Not(lhs) : lhs; }
    auto inst   = std::make_unique<NeInstruction<bool>>(lhs, rhs);
    auto result = inst->result = CurrentGroup()->Reserve();
    CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  }

  friend struct SetCurrent;
  friend struct SetTemporaries;

  struct State {
    internal::BlockGroup* group_ = nullptr;
    BasicBlock* block_;

    // Temporaries need to be destroyed at the end of each statement.
    // This is a pointer to a buffer where temporary allocations can register
    // themselves for deletion.
    std::vector<type::Typed<Reg>> temporaries_to_destroy_;
    bool more_stmts_allowed_ = true;
  } current_;
};

Builder& GetBuilder();

struct SetCurrent : public base::UseWithScope {
  SetCurrent(internal::BlockGroup* fn, Builder* builder = nullptr);
  ~SetCurrent();

 private:
  Builder* builder_;
  internal::BlockGroup* old_group_;
  BasicBlock* old_block_;
};

struct SetTemporaries : public base::UseWithScope {
  SetTemporaries(Builder& bldr) : bldr_(bldr) {
    old_temporaries_ = std::exchange(bldr_.current_.temporaries_to_destroy_,
                                     std::vector<type::Typed<Reg>>{});
    old_more_stmts_allowed_ =
        std::exchange(bldr_.current_.more_stmts_allowed_, true);
  }
  ~SetTemporaries() {}

 private:
  std::vector<type::Typed<Reg>> old_temporaries_;
  bool old_more_stmts_allowed_;
  Builder& bldr_;
};

template <typename ToType, typename FromType>
RegOr<ToType> Cast(RegOr<FromType> r) {
  if (r.is_reg()) {
    auto inst   = std::make_unique<CastInstruction<ToType, FromType>>(r);
    auto result = inst->result = GetBuilder().CurrentGroup()->Reserve();
    GetBuilder().CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  } else {
    return ToType(r.value());
  }
}

template <typename ToType>
RegOr<ToType> CastTo(type::Type const* from_type, ir::Results const& r) {
  if (from_type == type::Get<ToType>()) { return r.get<ToType>(0); }
  if (from_type == type::Int8) {
    return Cast<ToType, int8_t>(r.get<int8_t>(0));
  } else if (from_type == type::Nat8) {
    return Cast<ToType, uint8_t>(r.get<uint8_t>(0));
  } else if (from_type == type::Int16) {
    return Cast<ToType, int16_t>(r.get<int16_t>(0));
  } else if (from_type == type::Nat16) {
    return Cast<ToType, uint16_t>(r.get<uint16_t>(0));
  } else if (from_type == type::Int32) {
    return Cast<ToType, int32_t>(r.get<int32_t>(0));
  } else if (from_type == type::Nat32) {
    return Cast<ToType, uint32_t>(r.get<uint32_t>(0));
  } else if (from_type == type::Int64) {
    return Cast<ToType, int64_t>(r.get<int64_t>(0));
  } else if (from_type == type::Nat64) {
    return Cast<ToType, uint64_t>(r.get<uint64_t>(0));
  } else if (from_type == type::Float32) {
    return Cast<ToType, float>(r.get<float>(0));
  } else if (from_type == type::Float64) {
    return Cast<ToType, double>(r.get<double>(0));
  } else {
    UNREACHABLE();
  }
}

template <typename T>
base::Tagged<T, Reg> Load(RegOr<Addr> addr) {
  auto& blk = *GetBuilder().CurrentBlock();

  // TODO Just take a Reg. RegOr<Addr> is overkill and not possible because
  // constants don't have addresses.
  ASSERT(addr.is_reg() == true);
  auto& cache_results = blk.storage_cache_[addr.reg()];
  if (not cache_results.empty()) {
    // TODO may not be Reg. could be anything of the right type.
    return cache_results.get<Reg>(0);
  }

  auto inst   = std::make_unique<LoadInstruction>(addr.reg(),
                                                core::Bytes::Get<T>().value());
  auto result = inst->result = GetBuilder().CurrentGroup()->Reserve();
  cache_results.append(result);
  blk.instructions_.push_back(std::move(inst));
  return result;
}

inline Reg Load(RegOr<Addr> r, type::Type const* t) {
  using base::stringify;
  DEBUG_LOG("Load")("Calling Load(", stringify(r), ", ", t->to_string(), ")");
  if (t->is<type::Function>()) { return Load<AnyFunc>(r); }
  return type::ApplyTypes<bool, int8_t, int16_t, int32_t, int64_t, uint8_t,
                          uint16_t, uint32_t, uint64_t, float, double,
                          type::Type const*, EnumVal, FlagsVal, Addr,
                          std::string_view, AnyFunc>(t, [&](auto tag) -> Reg {
    using T = typename decltype(tag)::type;
    return Load<T>(r);
  });
}

template <typename T>
Reg MakeReg(T t) {
  static_assert(not std::is_same_v<T, Reg>);
  if constexpr (IsRegOr<T>::value) {
    auto inst   = std::make_unique<RegisterInstruction<typename T::type>>(t);
    auto result = inst->result = GetBuilder().CurrentGroup()->Reserve();
    GetBuilder().CurrentBlock()->instructions_.push_back(std::move(inst));
    return result;
  } else {
    return MakeReg(RegOr<T>{t});
  }
}

template <typename T>
void SetRet(uint16_t n, T val) {
  if constexpr (IsRegOr<T>::value) {
    auto inst =
        std::make_unique<SetReturnInstruction<typename T::type>>(n, val);
    GetBuilder().CurrentBlock()->instructions_.push_back(std::move(inst));
  } else if constexpr (base::IsTaggedV<T>) {
    static_assert(std::is_same_v<typename T::base_type, Reg>);
    SetRet(n, RegOr<typename T::tag_type>(val));
  } else {
    SetRet(n, RegOr<T>(val));
  }
}

inline base::Tagged<Addr, Reg> GetRet(uint16_t n, type::Type const* t) {
  auto inst   = std::make_unique<GetReturnInstruction>(n);
  auto result = inst->result = GetBuilder().CurrentGroup()->Reserve();
  GetBuilder().CurrentBlock()->instructions_.push_back(std::move(inst));
  return result;
}

inline void SetRet(uint16_t n, type::Typed<Results> const& r) {
  // if (r.type()->is<type::GenericStruct>()) {
  //   SetRet(n, r->get<AnyFunc>(0));
  // }
  if (r.type()->is<type::Jump>()) {
    // TODO currently this has to be implemented outside type::Apply because
    // that's in type.h which is wrong because it forces weird instantiation
    // order issues (type/type.h can't depend on type/jump.h).
    SetRet(n, r->get<AnyFunc>(0));
  } else {
    ASSERT(r.type()->is_big() == false) << r.type()->to_string();
    type::Apply(r.type(), [&](auto tag) {
      using T = typename decltype(tag)::type;
      SetRet(n, r->get<T>(0));
    });
  }
}

template <typename T>
void Store(T r, RegOr<Addr> addr) {
  if constexpr (IsRegOr<T>::value) {
    auto& blk = *GetBuilder().CurrentBlock();
    blk.storage_cache_.clear();
    auto inst = std::make_unique<StoreInstruction<typename T::type>>(r, addr);
    blk.instructions_.push_back(std::move(inst));
  } else {
    Store(RegOr<T>(r), addr);
  }
}

}  // namespace ir

#endif  // ICARUS_IR_BUILDER_H
