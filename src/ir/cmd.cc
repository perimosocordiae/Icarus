#include "ir/cmd.h"

#include <cmath>
#include <iostream>

#include "architecture.h"
#include "base/container/vector.h"
#include "ir/func.h"
#include "type/all.h"

namespace IR {
using base::check::Is;
BlockIndex BasicBlock::Current;

std::string LongArgs::to_string() const {
  std::stringstream ss;
  auto arch     = Architecture::InterprettingMachine();
  size_t offset = 0;
  size_t i      = 0;
  for (auto *t : type_->input) {
    offset = arch.MoveForwardToAlignment(t, offset);
    if (is_reg_[i]) {
      ss << " " << args_.get<Register>(offset).to_string();
      offset += sizeof(Register);
    } else {
      ss << " [??]";
      offset += arch.bytes(t);
    }
    ++i;
  }
  return ss.str();
}

void LongArgs::append(Register reg) {
  args_.append(reg);
  is_reg_.push_back(true);
}
void LongArgs::append(const IR::Val &val) {
  // TODO deal with alignment?
  std::visit(
      base::overloaded{
          [](const IR::Interface &) { UNREACHABLE(); },
          [&](auto &&val) {
            args_.append(val);
            is_reg_.push_back(
                std::is_same_v<IR::Register, std::decay_t<decltype(val)>>);
          }},
      val.value);
}

static Cmd &MakeCmd(const type::Type *t, Op op) {
  auto &cmd = ASSERT_NOT_NULL(Func::Current)
                  ->block(BasicBlock::Current)
                  .cmds_.emplace_back(t, op);
  return cmd;
}

RegisterOr<char> Trunc(RegisterOr<i32> r) {
  if (!r.is_reg_) { return static_cast<char>(r.val_); }
  auto &cmd  = MakeCmd(type::Char, Op::Trunc);
  cmd.trunc_ = Cmd::Trunc::Make(r.reg_);
  return cmd.result;
}

RegisterOr<i32> Extend(RegisterOr<char> r) {
  if (!r.is_reg_) { return static_cast<i32>(r.val_); }
  auto &cmd   = MakeCmd(type::Int, Op::Extend);
  cmd.extend_ = Cmd::Extend::Make(r.reg_);
  return cmd.result;
}

Register Bytes(RegisterOr<type::Type const *> r) {
  auto &cmd  = MakeCmd(type::Int, Op::Bytes);
  cmd.bytes_ = Cmd::Bytes::Make(r);
  return cmd.result;
}

Register Align(RegisterOr<type::Type const *> r) {
  auto &cmd  = MakeCmd(type::Int, Op::Align);
  cmd.align_ = Cmd::Align::Make(r);
  return cmd.result;
}

RegisterOr<bool> Not(RegisterOr<bool> r) {
  if (!r.is_reg_) { return !r.val_; }
  auto &cmd = MakeCmd(type::Bool, Op::Not);
  cmd.not_  = Cmd::Not::Make(r.reg_);
  Func::Current->references_[cmd.not_.reg_].insert(cmd.result);
  return cmd.result;
}

// TODO do you really want to support this? How can array allocation be
// customized?
Val Malloc(const type::Type *t, const Val &v) {
  auto &cmd         = MakeCmd(type::Ptr(t), Op::Malloc);
  Register const *r = std::get_if<Register>(&v.value);
  cmd.malloc_       = Cmd::Malloc::Make(r ? RegisterOr<i32>(*r)
                                    : RegisterOr<i32>(std::get<i32>(v.value)));
  return cmd.reg();
}

void Free(const Val &v) {
  auto &cmd = MakeCmd(v.type, Op::Free);
  cmd.free_ = Cmd::Free::Make(std::get<Register>(v.value));
}

RegisterOr<i32> NegInt(RegisterOr<i32> r) {
  if (!r.is_reg_) { return -r.val_; }
  auto &cmd = MakeCmd(type::Bool, Op::NegInt);
  cmd.neg_int_ = Cmd::NegInt::Make(r.reg_);
  Func::Current->references_[cmd.neg_int_.reg_].insert(cmd.result);
  return cmd.result;
}

RegisterOr<double> NegReal(RegisterOr<double> r) {
  if (!r.is_reg_) { return -r.val_; }
  auto &cmd     = MakeCmd(type::Bool, Op::NegReal);
  cmd.neg_real_ = Cmd::NegReal::Make(r.reg_);
  Func::Current->references_[cmd.neg_real_.reg_].insert(cmd.result);
  return cmd.result;
}

Register ArrayLength(Register r) {
  auto &cmd         = MakeCmd(type::Ptr(type::Int), Op::ArrayLength);
  cmd.array_length_ = Cmd::ArrayLength::Make(r);
  return cmd.result;
}

Register ArrayData(Register r, type::Type const *ptr) {
  auto *array_type = &ptr->as<type::Pointer>().pointee->as<type::Array>();
  ASSERT(!array_type->fixed_length);

  auto &cmd       = MakeCmd(type::Ptr(array_type->data_type), Op::ArrayData);
  cmd.array_data_ = Cmd::ArrayData::Make(r);
  return cmd.result;
}

Val Ptr(const Val &v) {
  ASSERT(v.type == type::Type_);
  if (type::Type const *const *t = std::get_if<const type::Type *>(&v.value)) {
    return Val::Type(type::Ptr(*t));
  }

  auto &cmd = MakeCmd(type::Type_, Op::Ptr);
  cmd.ptr_  = Cmd::Ptr::Make(std::get<Register>(v.value));
  return cmd.reg();
}

#define DEFINE_CMD1(Name, name, t)                                             \
  Register Name(Register r) {                                                  \
    auto &cmd = MakeCmd(t, Op::Name);                                          \
    cmd.name  = Cmd::Name::Make(r);                                            \
    return cmd.result;                                                         \
  }                                                                            \
  struct AllowSemicolon
DEFINE_CMD1(LoadBool, load_bool_, type::Bool);
DEFINE_CMD1(LoadChar, load_char_, type::Char);
DEFINE_CMD1(LoadInt, load_int_, type::Int);
DEFINE_CMD1(LoadReal, load_real_, type::Real);
DEFINE_CMD1(LoadType, load_type_, type::Type_);
#undef DEFINE_CMD1

#define DEFINE_CMD1(Name, name)                                                \
  Register Name(Register r, type::Type const *t) {                             \
    auto &cmd = MakeCmd(t, Op::Name);                                          \
    cmd.name  = Cmd::Name::Make(r);                                            \
    return cmd.result;                                                         \
  }                                                                            \
  struct AllowSemicolon
DEFINE_CMD1(LoadEnum, load_enum_);
DEFINE_CMD1(LoadFlags, load_flags_);
DEFINE_CMD1(LoadAddr, load_addr_);
#undef DEFINE_CMD1

#define DEFINE_CMD1(Name, name, arg_type, RetType)                             \
  Val Name(Val const &v) {                                                     \
    auto &cmd = MakeCmd(RetType, Op::Name);                                    \
    auto *x   = std::get_if<arg_type>(&v.value);                               \
    cmd.name  = Cmd::Name::Make(                                               \
        x ? RegisterOr<arg_type>(*x)                                          \
          : RegisterOr<arg_type>(std::get<Register>(v.value)));               \
    return cmd.reg();                                                          \
  }                                                                            \
  struct AllowSemicolon
DEFINE_CMD1(PrintBool, print_bool_, bool, type::Bool);
DEFINE_CMD1(PrintChar, print_char_, char, type::Char);
DEFINE_CMD1(PrintInt, print_int_, i32, type::Int);
DEFINE_CMD1(PrintReal, print_real_, double, type::Real);
DEFINE_CMD1(PrintType, print_type_, type::Type const *, type::Type_);
DEFINE_CMD1(PrintEnum, print_enum_, EnumVal, v.type);
DEFINE_CMD1(PrintFlags, print_flags_, FlagsVal, v.type);
DEFINE_CMD1(PrintAddr, print_addr_, IR::Addr, v.type);
DEFINE_CMD1(PrintCharBuffer, print_char_buffer_, std::string_view, v.type);
#undef DEFINE_CMD1

Val AddCharBuf(const Val &v1, const Val &v2) {
  auto *s1 = std::get_if<std::string_view>(&v1.value);
  auto *s2 = std::get_if<std::string_view>(&v2.value);
  if (s1 && s2) {
    return IR::Val::CharBuf(std::string(*s1) + std::string(*s2));
  }

  auto &cmd         = MakeCmd(nullptr /* TODO */, Op::AddCharBuf);
  cmd.add_char_buf_ = Cmd::AddCharBuf::Make(
      s1 ? RegisterOr<std::string_view>(*s1)
         : RegisterOr<std::string_view>(std::get<Register>(v1.value)),
      s2 ? RegisterOr<std::string_view>(*s2)
         : RegisterOr<std::string_view>(std::get<Register>(v2.value)));
  return cmd.reg();
}

#define DEFINE_CMD2(Name, name, arg_type, RetType, ret_type, fn)               \
  RegisterOr<ret_type> Name(RegisterOr<arg_type> v1,                           \
                            RegisterOr<arg_type> v2) {                         \
    if (!v1.is_reg_ && v2.is_reg_) { return fn(v1.val_, v2.val_); }            \
    auto &cmd  = MakeCmd(type::RetType, Op::Name);                             \
    cmd.name   = Cmd::Name::Make(v1, v2);                                      \
    auto &refs = Func::Current->references_;                                   \
    if (v1.is_reg_) { refs[v1.reg_].insert(cmd.result); }                      \
    if (v2.is_reg_) { refs[v2.reg_].insert(cmd.result); }                      \
    return cmd.result;                                                         \
  }                                                                            \
  struct AllowSemicolon
DEFINE_CMD2(AddInt, add_int_, i32, Int, i32, std::plus<i32>{});
DEFINE_CMD2(AddReal, add_real_, double, Real, double, std::plus<double>{});
DEFINE_CMD2(SubInt, sub_int_, i32, Int, i32, std::minus<i32>{});
DEFINE_CMD2(SubReal, sub_real_, double, Real, double, std::minus<double>{});
DEFINE_CMD2(MulInt, mul_int_, i32, Int, i32, std::multiplies<i32>{});
DEFINE_CMD2(MulReal, mul_real_, double, Real, double,
            std::multiplies<double>{});
DEFINE_CMD2(DivInt, div_int_, i32, Int, i32, std::divides<i32>{});
DEFINE_CMD2(DivReal, div_real_, double, Real, double, std::divides<double>{});
DEFINE_CMD2(ModInt, mod_int_, i32, Int, i32, std::modulus<i32>{});
DEFINE_CMD2(ModReal, mod_real_, double, Real, double, std::fmod);
DEFINE_CMD2(LtInt, lt_int_, i32, Bool, bool, std::less<i32>{});
DEFINE_CMD2(LtReal, lt_real_, double, Bool, bool, std::less<double>{});
DEFINE_CMD2(LtFlags, lt_flags_, FlagsVal, Bool, bool,
            [](FlagsVal lhs, FlagsVal rhs) {
              return lhs.value != rhs.value &&
                     ((lhs.value | rhs.value) == rhs.value);
            });
DEFINE_CMD2(LeInt, le_int_, i32, Bool, bool, std::less_equal<i32>{});
DEFINE_CMD2(LeReal, le_real_, double, Bool, bool, std::less_equal<double>{});
DEFINE_CMD2(LeFlags, le_flags_, FlagsVal, Bool, bool,
            [](FlagsVal lhs, FlagsVal rhs) {
              return (lhs.value | rhs.value) == rhs.value;
            });
DEFINE_CMD2(GtInt, gt_int_, i32, Bool, bool, std::less<i32>{});
DEFINE_CMD2(GtReal, gt_real_, double, Bool, bool, std::less<double>{});
DEFINE_CMD2(GtFlags, gt_flags_, FlagsVal, Bool, bool,
            [](FlagsVal lhs, FlagsVal rhs) {
              return lhs.value != rhs.value &&
                     ((lhs.value | rhs.value) == lhs.value);
            });
DEFINE_CMD2(GeInt, ge_int_, i32, Bool, bool, std::less_equal<i32>{});
DEFINE_CMD2(GeReal, ge_real_, double, Bool, bool, std::less_equal<double>{});
DEFINE_CMD2(GeFlags, ge_flags_, FlagsVal, Bool, bool,
            [](FlagsVal lhs, FlagsVal rhs) {
              return (lhs.value | rhs.value) == lhs.value;
            });
DEFINE_CMD2(EqChar, eq_char_, char, Bool, bool, std::equal_to<char>{});
DEFINE_CMD2(EqInt, eq_int_, i32, Bool, bool, std::equal_to<i32>{});
DEFINE_CMD2(EqReal, eq_real_, double, Bool, bool, std::equal_to<double>{});
DEFINE_CMD2(EqType, eq_type_, const type::Type *, Bool, bool,
            std::equal_to<const type::Type *>{});
DEFINE_CMD2(EqFlags, eq_flags_, FlagsVal, Bool, bool,
            std::equal_to<FlagsVal>{});
DEFINE_CMD2(EqAddr, eq_addr_, Addr, Bool, bool, std::equal_to<Addr>{});
DEFINE_CMD2(NeChar, ne_char_, char, Bool, bool, std::not_equal_to<char>{});
DEFINE_CMD2(NeInt, ne_int_, i32, Bool, bool, std::not_equal_to<i32>{});
DEFINE_CMD2(NeReal, ne_real_, double, Bool, bool, std::not_equal_to<double>{});
DEFINE_CMD2(NeType, ne_type_, const type::Type *, Bool, bool,
            std::not_equal_to<const type::Type *>{});
DEFINE_CMD2(NeFlags, ne_flags_, FlagsVal, Bool, bool,
            std::not_equal_to<FlagsVal>{});
DEFINE_CMD2(NeAddr, ne_addr_, Addr, Bool, bool, std::not_equal_to<Addr>{});
DEFINE_CMD2(Arrow, arrow_, type::Type const *, Type_, type::Type const *,
            [](type::Type const *lhs, type::Type const *rhs) {
              return type::Func({lhs}, {rhs});
            });
#undef DEFINE_CMD2

RegisterOr<bool> EqBool(RegisterOr<bool> v1, RegisterOr<bool> v2) {
  if (!v1.is_reg_) { return v1.val_ ? v2 : Not(v2); }
  if (!v2.is_reg_) { return v2.val_ ? v1 : Not(v1); }
  auto &cmd    = MakeCmd(type::Bool, Op::EqBool);
  cmd.eq_bool_ = Cmd::EqBool::Make(v1.reg_, v2.reg_);
  return cmd.result;
}

Val Array(const Val &v1, const Val &v2) {
  ASSERT(v2.type == type::Type_);

  i32 const *m               = std::get_if<i32>(&v1.value);
  type::Type const *const *t = std::get_if<const type::Type *>(&v2.value);
  if (t) {
    if (m) { return Val::Type(type::Arr(*t, *m)); }
    if (v1 == Val::None()) { return Val::Type(type::Arr(*t)); }
  }

  auto &cmd  = MakeCmd(type::Type_, Op::Array);
  cmd.array_ = Cmd::Array::Make(
      m ? RegisterOr<i32>(*m)
        : v1 == Val::None() ? RegisterOr<i32>(-1)
                            : RegisterOr<i32>(std::get<Register>(v1.value)),
      t ? RegisterOr<type::Type const *>(*t)
        : RegisterOr<type::Type const *>(std::get<Register>(v2.value)));
  return cmd.reg();
}

Register CreateTuple() { return MakeCmd(type::Type_, Op::CreateTuple).result; }

void AppendToTuple(Register tup, RegisterOr<type::Type const *> entry) {
  auto &cmd                  = MakeCmd(nullptr, Op::AppendToTuple);
  cmd.append_to_tuple_       = Cmd::AppendToTuple::Make(tup, entry);
}

Register FinalizeTuple(Register r) {
  auto &cmd           = MakeCmd(type::Type_, Op::FinalizeTuple);
  cmd.finalize_tuple_ = Cmd::FinalizeTuple::Make(r);
  return cmd.result;
}

Register Tup(base::vector<Val> const &entries) {
  IR::Register tup = IR::CreateTuple();
  for (auto const &val : entries) {
    IR::AppendToTuple(tup, val.reg_or<type::Type const *>());
  }
  return IR::FinalizeTuple(tup);
}

Register CreateVariant() {
  return MakeCmd(type::Type_, Op::CreateVariant).result;
}

void AppendToVariant(Register var, RegisterOr<type::Type const *> entry) {
  auto &cmd              = MakeCmd(nullptr, Op::AppendToVariant);
  cmd.append_to_variant_ = Cmd::AppendToVariant::Make(var, entry);
}

Register FinalizeVariant(Register r) {
  auto &cmd             = MakeCmd(type::Type_, Op::FinalizeVariant);
  cmd.finalize_variant_ = Cmd::FinalizeVariant::Make(r);
  return cmd.result;
}

RegisterOr<type::Type const *> Variant(base::vector<Val> const &vals) {
  if (std::all_of(vals.begin(), vals.end(), [](Val const& v) {
        return std::holds_alternative<type::Type const *>(v.value);
        })) {

    base::vector<type::Type const *> types;
    types.reserve(vals.size());
    for (Val const &v : vals) {
      types.push_back(std::get<type::Type const *>(v.value));
    }

    return type::Var(std::move(types));
  }
  IR::Register var = IR::CreateVariant();
  for (auto const &val : vals) {
    IR::AppendToVariant(var, val.reg_or<type::Type const *>());
  }
  return IR::FinalizeVariant(var);
}

RegisterOr<bool> XorBool(RegisterOr<bool> v1, RegisterOr<bool> v2) {
  if (!v1.is_reg_) { return v1.val_ ? Not(v2) : v2; }
  if (!v2.is_reg_) { return v2.val_ ? Not(v1) : v1; }
  auto &cmd     = MakeCmd(type::Bool, Op::XorBool);
  cmd.xor_bool_ = Cmd::XorBool::Make(v1, v2);
  return cmd.result;
}

Val Field(const Val &v, size_t n) {
  const type::Struct *struct_type =
      &v.type->as<type::Pointer>().pointee->as<type::Struct>();
  const type::Type *result_type = type::Ptr(struct_type->fields_.at(n).type);
  auto &cmd                     = MakeCmd(result_type, Op::Field);
  cmd.field_ = Cmd::Field::Make(std::get<Register>(v.value), struct_type, n);
  return cmd.reg();
}

static Register Reserve(type::Type const *t, bool incr_num_regs = true) {
  auto arch    = Architecture::InterprettingMachine();
  auto reg_val = arch.MoveForwardToAlignment(t, Func::Current->reg_size_);
  auto result  = Register(reg_val);
  Func::Current->reg_size_ = reg_val + arch.bytes(t);
  if (incr_num_regs) {
    Func::Current->reg_map_.emplace(Func::Current->num_regs_, result);
    ++Func::Current->num_regs_;
  }
  return result;
}

Cmd::Cmd(const type::Type *t, Op op) : op_code_(op), type(t) {
  ASSERT(Func::Current != nullptr);
  CmdIndex cmd_index{
      BasicBlock::Current,
      static_cast<i32>(Func::Current->block(BasicBlock::Current).cmds_.size())};
  if (t == nullptr) {
    result = Register{--Func::Current->neg_bound_};
    Func::Current->references_[result];  // Guarantee it exists.
    Func::Current->reg_to_cmd_.emplace(result, cmd_index);
    Func::Current->reg_map_.emplace(Func::Current->neg_bound_, result);
    return;
  }

  result = Reserve(t);
  Func::Current->references_[result];  // Guarantee it exists.
  // TODO for implicitly declared out-params of a Call, map them to the call.
  Func::Current->reg_to_cmd_.emplace(result, cmd_index);
}

Val OutParams::AppendReg(type::Type const *t) {
  auto reg = Reserve(t, false);
  outs_.emplace_back(reg, false);
  return IR::Val::Reg(reg, t);
}

BlockSequence MakeBlockSeq(base::vector<IR::BlockSequence> const &blocks);

Register CreateBlockSeq() { return MakeCmd(type::Type_, Op::CreateBlockSeq).result; }

void AppendToBlockSeq(Register block_seq,
                      RegisterOr<IR::BlockSequence> more_block_seq) {
  auto &cmd = MakeCmd(nullptr, Op::AppendToBlockSeq);
  cmd.append_to_block_seq_ =
      Cmd::AppendToBlockSeq::Make(block_seq, more_block_seq);
}

Register FinalizeBlockSeq(Register r) {
  auto &cmd           = MakeCmd(type::Block, Op::FinalizeBlockSeq);
  cmd.finalize_block_seq_ = Cmd::FinalizeBlockSeq::Make(r);
  return cmd.result;
}

// TODO replace Val with RegOr<BlockSequence>
Val BlockSeq(const base::vector<Val> &blocks) {
  if (std::all_of(blocks.begin(), blocks.end(), [](const IR::Val &v) {
        return std::holds_alternative<IR::BlockSequence>(v.value);
      })) {
    std::vector<IR::BlockSequence> block_seqs;
    block_seqs.reserve(blocks.size());
    for (const auto &val : blocks) {
      block_seqs.push_back(std::get<IR::BlockSequence>(val.value));
    }
    return IR::Val::BlockSeq(MakeBlockSeq(block_seqs));
  }

  auto reg = CreateBlockSeq();
  for (auto const &val : blocks) {
    IR::AppendToBlockSeq(reg, val.reg_or<IR::BlockSequence>());
  }
  // TODO can it be an opt block?
  return IR::Val::Reg(IR::FinalizeBlockSeq(reg), type::Block);
}

Val BlockSeqContains(const Val &v, AST::BlockLiteral *lit) {
  auto &cmd = MakeCmd(type::Bool, Op::BlockSeqContains);
  if (auto *bs = std::get_if<BlockSequence>(&v.value)) {
    return IR::Val::Bool(
        std::any_of(bs->seq_->begin(), bs->seq_->end(),
                    [lit](AST::BlockLiteral *l) { return lit == l; }));
  }
  cmd.block_seq_contains_ =
      Cmd::BlockSeqContains{{}, std::get<Register>(v.value), lit};
  return cmd.reg();
}

Val Cast(const type::Type *to, const Val &v, Context *ctx) {
  if (v.type == to) {
    // TODO lvalue/rvalue?
    return v;
  } else if (i32 const *n = std::get_if<i32>(&v.value); n && to == type::Real) {
    return Val::Real(static_cast<double>(*n));

  } else if (to->is<type::Variant>()) {
    ASSERT(ctx != nullptr);
    // TODO cleanup?
    auto alloc = IR::Val::Reg(Alloca(to), to);

    to->EmitAssign(v.type, std::move(v), alloc, ctx);
    return alloc;

  } else if (v.type->is<type::Pointer>()) {
    auto *ptee_type = v.type->as<type::Pointer>().pointee;
    if (ptee_type->is<type::Array>()) {
      auto &array_type = ptee_type->as<type::Array>();
      if (array_type.fixed_length && Ptr(array_type.data_type) == to) {
        Val v_copy  = v;
        v_copy.type = to;
        return v_copy;
      }
    }
  }

  if (to == type::Real && v.type == type::Int) {
    auto &cmd             = MakeCmd(type::Real, Op::CastIntToReal);
    cmd.cast_int_to_real_ = Cmd::CastIntToReal{{}, std::get<Register>(v.value)};
    return cmd.reg();
  } else if (to->is<type::Pointer>()) {
    auto &cmd     = MakeCmd(to, Op::CastPtr);
    cmd.cast_ptr_ = Cmd::CastPtr{
        {}, std::get<Register>(v.value), to->as<type::Pointer>().pointee};
    return cmd.reg();
  } else {
    UNREACHABLE();
  }
}

Register CreateStruct() { return MakeCmd(type::Type_, Op::CreateStruct).result; }

Register FinalizeStruct(Register r) {
  auto &cmd            = MakeCmd(type::Type_, Op::FinalizeStruct);
  cmd.finalize_struct_ = Cmd::FinalizeStruct::Make(r);
  return cmd.result;
}

Val VariantType(const Val &v) {
  auto &cmd         = MakeCmd(Ptr(type::Type_), Op::VariantType);
  cmd.variant_type_ = Cmd::VariantType::Make(std::get<Register>(v.value));
  return cmd.reg();
}

Val VariantValue(type::Type const *t, const Val &v) {
  auto &cmd         = MakeCmd(type::Ptr(t), Op::VariantValue);
  cmd.variant_type_ = Cmd::VariantType::Make(std::get<Register>(v.value));
  return cmd.reg();
}

void CreateStructField(Register struct_type,
                       RegisterOr<type::Type const *> type) {
  auto &cmd = MakeCmd(nullptr, Op::CreateStructField);
  cmd.create_struct_field_ =
      Cmd::CreateStructField::Make(struct_type, std::move(type));
}

void SetStructFieldName(Register struct_type, std::string_view field_name) {
  auto &cmd                  = MakeCmd(nullptr, Op::SetStructFieldName);
  cmd.set_struct_field_name_ = Cmd::SetStructFieldName::Make(struct_type, field_name);
}

Register Alloca(const type::Type *t) {
  ASSERT(t, Not(Is<type::Tuple>()));
  return std::get<IR::Register>(
      ASSERT_NOT_NULL(Func::Current)
          ->block(Func::Current->entry())
          .cmds_.emplace_back(type::Ptr(t), Op::Alloca)
          .reg()
          .value);
}

void SetReturn(size_t n, Val const &v2) {
  if (v2.type == type::Bool) { return SetReturnBool(n, v2); }
  if (v2.type == type::Char) { return SetReturnChar(n, v2); }
  if (v2.type == type::Int) { return SetReturnInt(n, v2); }
  if (v2.type == type::Real) { return SetReturnReal(n, v2); }
  if (v2.type == type::Type_) { return SetReturnType(n, v2); }
  if (v2.type->is<type::Enum>()) { return SetReturnEnum(n, v2); }
  if (v2.type->is<type::Flags>()) { return SetReturnFlags(n, v2); }
  if (v2.type->is<type::CharBuffer>()) { return SetReturnCharBuf(n, v2); }
  if (v2.type->is<type::Pointer>()) { return SetReturnAddr(n, v2); }
  if (v2.type->is<type::Function>()) { return SetReturnFunc(n, v2); }
  if (v2.type->is<type::Scope>()) { return SetReturnScope(n, v2); }
  if (v2.type == type::Module) { return SetReturnModule(n, v2); }
  if (v2.type == type::Generic) { return SetReturnGeneric(n, v2); }
  if (v2.type == type::Block || v2.type == type::OptBlock) {
    return SetReturnBlock(n, v2);
  }
  UNREACHABLE(v2.type->to_string());
}

Val PtrIncr(const Val &v1, const Val &v2) {
  ASSERT(v1.type, Is<type::Pointer>());
  ASSERT(v2.type == type::Int);
  i32 const *n = std::get_if<i32>(&v2.value);
  if (n && *n == 0) { return v1; }
  auto &cmd     = MakeCmd(v1.type, Op::PtrIncr);
  cmd.ptr_incr_ = Cmd::PtrIncr::Make(
      std::get<Register>(v1.value),
      n ? RegisterOr<i32>(*n) : RegisterOr<i32>(std::get<i32>(v2.value)));
  return cmd.reg();
}

Val XorFlags(type::Flags const *type, RegisterOr<FlagsVal> const &lhs,
             RegisterOr<FlagsVal> const &rhs) {
  if (!lhs.is_reg_ && !rhs.is_reg_) {
    return Val::Flags(type, lhs.val_ ^ rhs.val_);
  }
  auto &cmd      = MakeCmd(type, Op::XorFlags);
  cmd.xor_flags_ = Cmd::XorFlags::Make(lhs, rhs);
  return cmd.reg();
}

Val OrFlags(type::Flags const *type, RegisterOr<FlagsVal> const &lhs,
            RegisterOr<FlagsVal> const &rhs) {
  if (!lhs.is_reg_ && !rhs.is_reg_) {
    return Val::Flags(type, lhs.val_ | rhs.val_);
  }
  auto &cmd     = MakeCmd(type, Op::OrFlags);
  cmd.or_flags_ = Cmd::OrFlags::Make(lhs, rhs);
  return cmd.reg();
}

Val AndFlags(type::Flags const *type, RegisterOr<FlagsVal> const &lhs,
             RegisterOr<FlagsVal> const &rhs) {
  if (!lhs.is_reg_ && !rhs.is_reg_) {
    return Val::Flags(type, lhs.val_ & rhs.val_);
  }
  auto &cmd      = MakeCmd(type, Op::AndFlags);
  cmd.and_flags_ = Cmd::AndFlags::Make(lhs, rhs);
  return cmd.reg();
}

Val Xor(const Val &v1, const Val &v2) {
  if (v1.type == type::Bool) {
    return ValFrom(XorBool(v1.reg_or<bool>(), v2.reg_or<bool>()));
  }
  if (v1.type->is<type::Flags>()) {
    return XorFlags(&v1.type->as<type::Flags>(), v1.reg_or<FlagsVal>(),
                    v2.reg_or<FlagsVal>());
  }
  UNREACHABLE();
}

Val Add(const Val &v1, const Val &v2) {
  if (v1.type == type::Int) {
    return ValFrom(AddInt(v1.reg_or<i32>(), v2.reg_or<i32>()));
  }
  if (v1.type == type::Real) {
    return ValFrom(AddReal(v1.reg_or<double>(), v2.reg_or<double>()));
  }
  if (v1.type->is<type::CharBuffer>()) { return AddCharBuf(v1, v2); }
  UNREACHABLE();
}

Val AddCodeBlock(const Val &v1, const Val &v2) { NOT_YET(); }

Val Sub(const Val &v1, const Val &v2) {
  if (v1.type == type::Int) {
    return ValFrom(SubInt(v1.reg_or<i32>(), v2.reg_or<i32>()));
  }
  if (v1.type == type::Real) {
    return ValFrom(SubReal(v1.reg_or<double>(), v2.reg_or<double>()));
  }
  UNREACHABLE();
}

Val Mul(const Val &v1, const Val &v2) {
  if (v1.type == type::Int) {
    return ValFrom(MulInt(v1.reg_or<i32>(), v2.reg_or<i32>()));
  }
  if (v1.type == type::Real) {
    return ValFrom(MulReal(v1.reg_or<double>(), v2.reg_or<double>()));
  }
  UNREACHABLE();
}

Val Div(const Val &v1, const Val &v2) {
  if (v1.type == type::Int) {
    return ValFrom(DivInt(v1.reg_or<i32>(), v2.reg_or<i32>()));
  }
  if (v1.type == type::Real) {
    return ValFrom(DivReal(v1.reg_or<double>(), v2.reg_or<double>()));
  }
  UNREACHABLE();
}

Val Mod(const Val &v1, const Val &v2) {
  if (v1.type == type::Int) {
    return ValFrom(ModInt(v1.reg_or<i32>(), v2.reg_or<i32>()));
  }
  if (v1.type == type::Real) {
    return ValFrom(ModReal(v1.reg_or<double>(), v2.reg_or<double>()));
  }
  UNREACHABLE();
}

void StoreBool(RegisterOr<bool> val, Register loc) {
  auto &cmd       = MakeCmd(nullptr, Op::StoreBool);
  cmd.store_bool_ = Cmd::StoreBool::Make(loc, val);
}

void StoreChar(RegisterOr<char> val, Register loc) {
  auto &cmd       = MakeCmd(nullptr, Op::StoreChar);
  cmd.store_char_ = Cmd::StoreChar::Make(loc, val);
}

void StoreInt(RegisterOr<i32> val, Register loc) {
  auto &cmd      = MakeCmd(nullptr, Op::StoreInt);
  cmd.store_int_ = Cmd::StoreInt::Make(loc, val);
}

void StoreReal(RegisterOr<double> val, Register loc) {
  auto &cmd       = MakeCmd(nullptr, Op::StoreReal);
  cmd.store_real_ = Cmd::StoreReal::Make(loc, val);
}

void StoreType(RegisterOr<type::Type const *> val, Register loc) {
  auto &cmd       = MakeCmd(nullptr, Op::StoreType);
  cmd.store_type_ = Cmd::StoreType::Make(loc, val);
}

void StoreEnum(RegisterOr<EnumVal> val, Register loc) {
  auto &cmd       = MakeCmd(nullptr, Op::StoreEnum);
  cmd.store_enum_ = Cmd::StoreEnum::Make(loc, val);
}

void StoreFlags(RegisterOr<FlagsVal> val, Register loc) {
  auto &cmd        = MakeCmd(nullptr, Op::StoreFlags);
  cmd.store_flags_ = Cmd::StoreFlags::Make(loc, val);
}

void StoreAddr(RegisterOr<IR::Addr> val, Register loc) {
  auto &cmd       = MakeCmd(nullptr, Op::StoreAddr);
  cmd.store_addr_ = Cmd::StoreAddr::Make(loc, val);
}

void SetReturnBool(size_t n, const Val &v2) {
  auto &cmd            = MakeCmd(nullptr, Op::SetReturnBool);
  auto reg_or = v2.reg_or<bool>();
  cmd.set_return_bool_ = Cmd::SetReturnBool::Make(n, reg_or);
  if (reg_or.is_reg_) {
    Func::Current->references_[reg_or.reg_].insert(cmd.result);
  }
}

void SetReturnChar(size_t n, const Val &v2) {
  auto &cmd            = MakeCmd(nullptr, Op::SetReturnChar);
  cmd.set_return_char_ = Cmd::SetReturnChar::Make(n, v2.reg_or<char>());
}

void SetReturnInt(size_t n, const Val &v2) {
  auto &cmd           = MakeCmd(nullptr, Op::SetReturnInt);
  cmd.set_return_int_ = Cmd::SetReturnInt::Make(n, v2.reg_or<i32>());
}

void SetReturnCharBuf(size_t n, const Val &v2) {
  auto &cmd = MakeCmd(nullptr, Op::SetReturnCharBuf);
  cmd.set_return_char_buf_ =
      Cmd::SetReturnCharBuf::Make(n, v2.reg_or<std::string_view>());
}

void SetReturnReal(size_t n, const Val &v2) {
  auto &cmd            = MakeCmd(nullptr, Op::SetReturnReal);
  cmd.set_return_real_ = Cmd::SetReturnReal::Make(n, v2.reg_or<double>());
}

void SetReturnType(size_t n, const Val &v2) {
  auto &cmd = MakeCmd(nullptr, Op::SetReturnType);
  cmd.set_return_type_ =
      Cmd::SetReturnType::Make(n, v2.reg_or<type::Type const *>());
}

void SetReturnEnum(size_t n, const Val &v2) {
  auto &cmd            = MakeCmd(nullptr, Op::SetReturnEnum);
  cmd.set_return_enum_ = Cmd::SetReturnEnum::Make(n, v2.reg_or<EnumVal>());
}

void SetReturnFlags(size_t n, const Val &v2) {
  auto &cmd             = MakeCmd(nullptr, Op::SetReturnFlags);
  cmd.set_return_flags_ = Cmd::SetReturnFlags::Make(n, v2.reg_or<FlagsVal>());
}

void SetReturnAddr(size_t n, const Val &v2) {
  auto &cmd            = MakeCmd(nullptr, Op::SetReturnAddr);
  cmd.set_return_addr_ = Cmd::SetReturnAddr::Make(n, v2.reg_or<IR::Addr>());
}

void SetReturnFunc(size_t n, const Val &v2) {
  auto &cmd            = MakeCmd(nullptr, Op::SetReturnFunc);
  cmd.set_return_func_ = Cmd::SetReturnFunc::Make(
      n,
      std::visit(
          base::overloaded{
              [](IR::Func *f) -> RegisterOr<AnyFunc> { return AnyFunc{f}; },
              [](IR::Register r) -> RegisterOr<AnyFunc> { return r; },
              [](IR::ForeignFn f) -> RegisterOr<AnyFunc> { return AnyFunc{f}; },
              [n](auto &&) -> RegisterOr<AnyFunc> { UNREACHABLE(); }},
          v2.value));
}

void SetReturnScope(size_t n, const Val &v2) {
  auto &cmd = MakeCmd(nullptr, Op::SetReturnScope);
  cmd.set_return_scope_ =
      Cmd::SetReturnScope::Make(n, v2.reg_or<AST::ScopeLiteral *>());
}

void SetReturnModule(size_t n, const Val &v2) {
  auto &cmd = MakeCmd(nullptr, Op::SetReturnModule);
  cmd.set_return_module_ =
      Cmd::SetReturnModule::Make(n, v2.reg_or<Module const *>());
}

void SetReturnGeneric(size_t n, const Val &v2) {
  auto &cmd = MakeCmd(nullptr, Op::SetReturnGeneric);
  cmd.set_return_generic_ =
      Cmd::SetReturnGeneric::Make(n, v2.reg_or<AST::Function *>());
}

void SetReturnBlock(size_t n, const Val &v2) {
  auto &cmd = MakeCmd(nullptr, Op::SetReturnBlock);
  cmd.set_return_block_ =
      Cmd::SetReturnBlock::Make(n, v2.reg_or<BlockSequence>());
}

void Store(const Val &val, Register loc) {
  if (val.type == type::Bool) { return StoreBool(val.reg_or<bool>(), loc); }
  if (val.type == type::Char) { return StoreChar(val.reg_or<char>(), loc); }
  if (val.type == type::Int) { return StoreInt(val.reg_or<i32>(), loc); }
  if (val.type == type::Real) { return StoreReal(val.reg_or<double>(), loc); }
  if (val.type == type::Type_) {
    return StoreType(val.reg_or<type::Type const *>(), loc);
  }
  if (val.type->is<type::Enum>()) {
    return StoreEnum(val.reg_or<EnumVal>(), loc);
  }
  if (val.type->is<type::Flags>()) {
    return StoreFlags(val.reg_or<FlagsVal>(), loc);
  }
  if (val.type->is<type::Pointer>()) {
    return StoreAddr(val.reg_or<IR::Addr>(), loc);
  }
  UNREACHABLE(val.type);
}

Val Load(const Val &v) {
  auto *ptee = v.type->as<type::Pointer>().pointee;
  return IR::Val::Reg(
      [&](Register r) {
        if (ptee == type::Bool) { return LoadBool(r); }
        if (ptee == type::Char) { return LoadChar(r); }
        if (ptee == type::Int) { return LoadInt(r); }
        if (ptee == type::Real) { return LoadReal(r); }
        if (ptee == type::Type_) { return LoadType(r); }
        if (ptee->is<type::Enum>()) { return LoadEnum(r, ptee); }
        if (ptee->is<type::Flags>()) { return LoadFlags(r, ptee); }
        if (ptee->is<type::Pointer>()) { return LoadAddr(r, ptee); }
        UNREACHABLE(v.type->to_string());
      }(std::get<Register>(v.value)),
      ptee);
}

Val Print(const Val &v) {
  if (v.type == type::Bool) { return PrintBool(v); }
  if (v.type == type::Char) { return PrintChar(v); }
  if (v.type == type::Int) { return PrintInt(v); }
  if (v.type == type::Real) { return PrintReal(v); }
  if (v.type == type::Type_) { return PrintType(v); }
  if (v.type->is<type::Enum>()) { return PrintEnum(v); }
  if (v.type->is<type::Flags>()) { return PrintFlags(v); }
  if (v.type->is<type::Pointer>()) { return PrintAddr(v); }
  if (v.type->is<type::CharBuffer>()) { return PrintCharBuffer(v); }
  UNREACHABLE(v.type->to_string());
}

Val Index(const Val &v1, const Val &v2) {
  ASSERT(v1.type, Is<type::Pointer>());
  ASSERT(v2.type == type::Int);
  auto *array_type = &v1.type->as<type::Pointer>().pointee->as<type::Array>();
  // TODO this works but generates worse IR (both here and in llvm). It's worth
  // figuring out how to do this better.
  return PtrIncr(
      Cast(type::Ptr(array_type->data_type),
           array_type->fixed_length
               ? v1
               : Load(IR::Val::Reg(
                     ArrayData(std::get<Register>(v1.value), v1.type),
                     type::Ptr(v1.type->as<type::Pointer>()
                                   .pointee->as<type::Array>()
                                   .data_type))),
           nullptr),
      v2);
}

Val Lt(const Val &v1, const Val &v2) {
  if (v1.type == type::Int) {
    return ValFrom(LtInt(v1.reg_or<i32>(), v2.reg_or<i32>()));
  }
  if (v1.type == type::Real) {
    return ValFrom(LtReal(v1.reg_or<double>(), v2.reg_or<double>()));
  }
  if (v1.type->is<type::Flags>()) {
    return ValFrom(LtFlags(v1.reg_or<FlagsVal>(), v2.reg_or<FlagsVal>()));
  }
  UNREACHABLE();
}

Val Le(const Val &v1, const Val &v2) {
  if (v1.type == type::Int) {
    return ValFrom(LeInt(v1.reg_or<i32>(), v2.reg_or<i32>()));
  }
  if (v1.type == type::Real) {
    return ValFrom(LeReal(v1.reg_or<double>(), v2.reg_or<double>()));
  }
  if (v1.type->is<type::Flags>()) {
    return ValFrom(LeFlags(v1.reg_or<FlagsVal>(), v2.reg_or<FlagsVal>()));
  }
  UNREACHABLE();
}

Val Gt(const Val &v1, const Val &v2) {
  if (v1.type == type::Int) {
    return ValFrom(GtInt(v1.reg_or<i32>(), v2.reg_or<i32>()));
  }
  if (v1.type == type::Real) {
    return ValFrom(GtReal(v1.reg_or<double>(), v2.reg_or<double>()));
  }
  if (v1.type->is<type::Flags>()) {
    return ValFrom(GtFlags(v1.reg_or<FlagsVal>(), v2.reg_or<FlagsVal>()));
  }
  UNREACHABLE();
}

Val Ge(const Val &v1, const Val &v2) {
  if (v1.type == type::Int) {
    return ValFrom(GeInt(v1.reg_or<i32>(), v2.reg_or<i32>()));
  }
  if (v1.type == type::Real) {
    return ValFrom(GeReal(v1.reg_or<double>(), v2.reg_or<double>()));
  }
  if (v1.type->is<type::Flags>()) {
    return ValFrom(GeFlags(v1.reg_or<FlagsVal>(), v2.reg_or<FlagsVal>()));
  }
  UNREACHABLE();
}

Val Eq(const Val &v1, const Val &v2) {
  if (v1.type == type::Bool) {
    return ValFrom(EqBool(v1.reg_or<bool>(), v2.reg_or<bool>()));
  }
  if (v1.type == type::Char) {
    return ValFrom(EqChar(v1.reg_or<char>(), v2.reg_or<char>()));
  }
  if (v1.type == type::Int) {
    return ValFrom(EqInt(v1.reg_or<i32>(), v2.reg_or<i32>()));
  }
  if (v1.type == type::Real) {
    return ValFrom(EqReal(v1.reg_or<double>(), v2.reg_or<double>()));
  }
  if (v1.type == type::Type_) {
    return ValFrom(EqType(v1.reg_or<type::Type const *>(),
                          v2.reg_or<type::Type const *>()));
  }
  if (v1.type->is<type::Flags>()) {
    return ValFrom(EqFlags(v1.reg_or<FlagsVal>(), v2.reg_or<FlagsVal>()));
  }
  if (v1.type->is<type::Pointer>()) {
    return ValFrom(EqAddr(v1.reg_or<IR::Addr>(), v2.reg_or<IR::Addr>()));
  }

  BlockSequence const *val1 = std::get_if<BlockSequence>(&v1.value);
  BlockSequence const *val2 = std::get_if<BlockSequence>(&v2.value);
  if (val1 != nullptr && val2 != nullptr) { return Val::Bool(*val1 == *val2); }

  // TODO block sequence at runtime?
  UNREACHABLE();
}

Val Ne(const Val &v1, const Val &v2) {
  if (v1.type == type::Bool) {
    return ValFrom(XorBool(v1.reg_or<bool>(), v2.reg_or<bool>()));
  }
  if (v1.type == type::Char) {
    return ValFrom(NeChar(v1.reg_or<char>(), v2.reg_or<char>()));
  }
  if (v1.type == type::Int) {
    return ValFrom(NeInt(v1.reg_or<i32>(), v2.reg_or<i32>()));
  }
  if (v1.type == type::Real) {
    return ValFrom(NeReal(v1.reg_or<double>(), v2.reg_or<double>()));
  }
  if (v1.type == type::Type_) {
    return ValFrom(NeType(v1.reg_or<type::Type const *>(),
                          v2.reg_or<type::Type const *>()));
  }
  if (v1.type->is<type::Pointer>()) {
    return ValFrom(NeAddr(v1.reg_or<IR::Addr>(), v2.reg_or<IR::Addr>()));
  }
  UNREACHABLE();
}

template <typename T>
static std::unique_ptr<PhiArgs<T>> MakePhiArgs(
    const std::unordered_map<BlockIndex, IR::Val> &val_map) {
  auto phi_args = std::make_unique<PhiArgs<T>>();
  for (const auto & [ block, val ] : val_map) {
    phi_args->map_.emplace(block, val.template reg_or<T>());
  }
  return phi_args;
}

CmdIndex Phi(type::Type const *t) {
  CmdIndex cmd_index{
      BasicBlock::Current,
      static_cast<i32>(Func::Current->block(BasicBlock::Current).cmds_.size())};
  MakeCmd(t, Op::Death);
  return cmd_index;
}

Val MakePhi(CmdIndex phi_index,
            const std::unordered_map<BlockIndex, IR::Val> &val_map) {
  auto &cmd = IR::Func::Current->Command(phi_index);
  cmd.type  = val_map.begin()->second.type;

  if (cmd.type == type::Bool) {
    auto phi_args = MakePhiArgs<bool>(val_map);
    cmd.op_code_  = Op::PhiBool;
    cmd.phi_bool_ = Cmd::PhiBool::Make(phi_args.get());
    IR::Func::Current->block(BasicBlock::Current)
        .phi_args_.push_back(std::move(phi_args));
  } else if (cmd.type == type::Char) {
    auto phi_args = MakePhiArgs<char>(val_map);
    cmd.op_code_  = Op::PhiChar;
    cmd.phi_char_ = Cmd::PhiChar::Make(phi_args.get());
    IR::Func::Current->block(BasicBlock::Current)
        .phi_args_.push_back(std::move(phi_args));
  } else if (cmd.type == type::Int) {
    auto phi_args = MakePhiArgs<i32>(val_map);
    cmd.op_code_  = Op::PhiInt;
    cmd.phi_int_  = Cmd::PhiInt::Make(phi_args.get());
    IR::Func::Current->block(BasicBlock::Current)
        .phi_args_.push_back(std::move(phi_args));
  } else if (cmd.type == type::Real) {
    auto phi_args = MakePhiArgs<double>(val_map);
    cmd.op_code_  = Op::PhiReal;
    cmd.phi_real_ = Cmd::PhiReal::Make(phi_args.get());
    IR::Func::Current->block(BasicBlock::Current)
        .phi_args_.push_back(std::move(phi_args));
  } else if (cmd.type == type::Type_) {
    auto phi_args = MakePhiArgs<type::Type const *>(val_map);
    cmd.op_code_  = Op::PhiType;
    cmd.phi_type_ = Cmd::PhiType::Make(phi_args.get());
    IR::Func::Current->block(BasicBlock::Current)
        .phi_args_.push_back(std::move(phi_args));
  } else if (cmd.type->is<type::Pointer>()) {
    auto phi_args = MakePhiArgs<IR::Addr>(val_map);
    cmd.op_code_  = Op::PhiAddr;
    cmd.phi_addr_ = Cmd::PhiAddr::Make(phi_args.get());
    IR::Func::Current->block(BasicBlock::Current)
        .phi_args_.push_back(std::move(phi_args));
  } else if (cmd.type == type::Block || cmd.type == type::OptBlock) {
    auto phi_args  = MakePhiArgs<BlockSequence>(val_map);
    cmd.op_code_   = Op::PhiBlock;
    cmd.phi_block_ = Cmd::PhiBlock::Make(phi_args.get());
    IR::Func::Current->block(BasicBlock::Current)
        .phi_args_.push_back(std::move(phi_args));
  } else {
    NOT_YET(cmd.type->to_string());
  }
  return cmd.reg();
}

void Call(const Val &fn, LongArgs long_args) {
  ASSERT(long_args.type_ == nullptr);
  ASSERT(fn.type, Is<type::Function>());
  long_args.type_     = &fn.type->as<type::Function>();
  const auto &fn_type = fn.type->as<type::Function>();

  auto &block    = Func::Current->block(BasicBlock::Current);
  LongArgs *args = &block.long_args_.emplace_back(std::move(long_args));

  auto &cmd = MakeCmd(nullptr, Op::Call);
  if (auto *r = std::get_if<Register>(&fn.value)) {
    cmd.call_ = Cmd::Call(*r, args, nullptr);
  } else if (auto *f = std::get_if<Func *>(&fn.value)) {
    cmd.call_ = Cmd::Call(*f, args, nullptr);
  } else if (auto *f = std::get_if<ForeignFn>(&fn.value)) {
    cmd.call_ = Cmd::Call(*f, args, nullptr);
  } else {
    UNREACHABLE();
  }
}
void Call(const Val &fn, LongArgs long_args, OutParams outs) {
  ASSERT(long_args.type_ == nullptr);
  ASSERT(fn.type, Is<type::Function>());
  long_args.type_     = &fn.type->as<type::Function>();
  const auto &fn_type = fn.type->as<type::Function>();

  auto &block = Func::Current->block(BasicBlock::Current);
  auto *args  = &block.long_args_.emplace_back(std::move(long_args));

  auto *outs_ptr = &block.outs_.emplace_back(std::move(outs));

  auto &cmd = MakeCmd(nullptr, Op::Call);
  if (auto *r = std::get_if<Register>(&fn.value)) {
    cmd.call_ = Cmd::Call(*r, args, outs_ptr);
  } else if (auto *f = std::get_if<Func *>(&fn.value)) {
    cmd.call_ = Cmd::Call(*f, args, outs_ptr);
  } else if (auto *f = std::get_if<ForeignFn>(&fn.value)) {
    cmd.call_ = Cmd::Call(*f, args, outs_ptr);
  } else {
    UNREACHABLE();
  }
}

void CondJump(const Val &cond, BlockIndex true_block, BlockIndex false_block) {
  if (auto *b = std::get_if<bool>(&cond.value)) {
    return UncondJump(*b ? true_block : false_block);
  }
  auto &cmd      = MakeCmd(nullptr, Op::CondJump);
  cmd.cond_jump_ = Cmd::CondJump{
      {}, std::get<Register>(cond.value), {false_block, true_block}};
}

void UncondJump(BlockIndex block) {
  auto &cmd        = MakeCmd(nullptr, Op::UncondJump);
  cmd.uncond_jump_ = Cmd::UncondJump{{}, block};
}

void ReturnJump() {
  auto &cmd        = MakeCmd(nullptr, Op::ReturnJump);
  cmd.return_jump_ = Cmd::ReturnJump{};
}

static std::ostream &operator<<(std::ostream &os, Register r) {
  return os << "reg." << r.value;
}

static std::ostream &operator<<(std::ostream &os, Addr addr) {
  return os << addr.to_string();
}

static std::ostream &operator<<(std::ostream &os, FlagsVal f) {
  return os << f.value;
}

static std::ostream &operator<<(std::ostream &os, EnumVal e) {
  return os << e.value;
}

static std::ostream &operator<<(std::ostream &os, BlockIndex b) {
  return os << "block." << b.value;
}

template <typename T>
static std::ostream &operator<<(std::ostream &os, RegisterOr<T> r) {
  if (r.is_reg_) {
    return os << r.reg_;
  } else {
    return os << r.val_;
  }
}

template <typename T>
static std::ostream &operator<<(std::ostream &os,
                                std::array<RegisterOr<T>, 2> r) {
  return os << r[0] << " " << r[1];
}

char const *OpCodeStr(Op op) {
  switch (op) {
#define OP_MACRO(op)                                                           \
  case Op::op:                                                                 \
    return #op;
#include "ir/op.xmacro.h"
#undef OP_MACRO
  }
  __builtin_unreachable();
}

std::ostream &operator<<(std::ostream &os, Cmd const &cmd) {
  if (cmd.result.value >= 0) { os << cmd.result << " = "; }
  os << OpCodeStr(cmd.op_code_) << " ";
  switch (cmd.op_code_) {
    case Op::Trunc: return os << cmd.trunc_.reg_;
    case Op::Extend: return os << cmd.extend_.reg_;
    case Op::Bytes: return os << cmd.bytes_.arg_;
    case Op::Align: return os << cmd.align_.arg_;
    case Op::Not: return os << cmd.not_.reg_;
    case Op::NegInt: return os << cmd.neg_int_.reg_;
    case Op::NegReal: return os << cmd.neg_real_.reg_;
    case Op::ArrayLength: return os << cmd.array_length_.arg_;
    case Op::ArrayData: return os << cmd.array_data_.arg_;
    case Op::Ptr: return os << cmd.ptr_.reg_;
    case Op::LoadBool: return os << cmd.load_bool_.arg_;
    case Op::LoadChar: return os << cmd.load_char_.arg_;
    case Op::LoadInt: return os << cmd.load_int_.arg_;
    case Op::LoadReal: return os << cmd.load_real_.arg_;
    case Op::LoadType: return os << cmd.load_type_.arg_;
    case Op::LoadEnum: return os << cmd.load_enum_.arg_;
    case Op::LoadFlags: return os << cmd.load_type_.arg_;
    case Op::LoadAddr: return os << cmd.load_addr_.arg_;
    case Op::PrintBool: return os << cmd.print_bool_.arg_;
    case Op::PrintChar: return os << cmd.print_char_.arg_;
    case Op::PrintInt: return os << cmd.print_int_.arg_;
    case Op::PrintReal: return os << cmd.print_real_.arg_;
    case Op::PrintType: return os << cmd.print_type_.arg_;
    case Op::PrintEnum: return os << cmd.print_enum_.arg_;
    case Op::PrintFlags: return os << cmd.print_flags_.arg_;
    case Op::PrintAddr: return os << cmd.print_addr_.arg_;
    case Op::PrintCharBuffer: return os << cmd.print_char_buffer_.arg_;
    case Op::AddInt: return os << cmd.add_int_.args_;
    case Op::AddReal: return os << cmd.add_real_.args_;
    case Op::AddCharBuf: return os << cmd.add_char_buf_.args_;
    case Op::SubInt: return os << cmd.sub_int_.args_;
    case Op::SubReal: return os << cmd.sub_real_.args_;
    case Op::MulInt: return os << cmd.mul_int_.args_;
    case Op::MulReal: return os << cmd.mul_real_.args_;
    case Op::DivInt: return os << cmd.div_int_.args_;
    case Op::DivReal: return os << cmd.div_real_.args_;
    case Op::ModInt: return os << cmd.mod_int_.args_;
    case Op::ModReal: return os << cmd.mod_real_.args_;
    case Op::LtInt: return os << cmd.lt_int_.args_;
    case Op::LtReal: return os << cmd.lt_real_.args_;
    case Op::LtFlags: return os << cmd.lt_flags_.args_;
    case Op::LeInt: return os << cmd.le_int_.args_;
    case Op::LeReal: return os << cmd.le_real_.args_;
    case Op::LeFlags: return os << cmd.le_flags_.args_;
    case Op::GtInt: return os << cmd.gt_int_.args_;
    case Op::GtReal: return os << cmd.gt_real_.args_;
    case Op::GtFlags: return os << cmd.gt_flags_.args_;
    case Op::GeInt: return os << cmd.ge_int_.args_;
    case Op::GeReal: return os << cmd.ge_real_.args_;
    case Op::GeFlags: return os << cmd.ge_flags_.args_;
    case Op::EqBool: return os << cmd.eq_bool_.args_[0] << " " << cmd.eq_bool_.args_[1];
    case Op::EqChar: return os << cmd.eq_char_.args_;
    case Op::EqInt: return os << cmd.eq_int_.args_;
    case Op::EqReal: return os << cmd.eq_real_.args_;
    case Op::EqFlags: return os << cmd.eq_flags_.args_;
    case Op::EqType: return os << cmd.eq_type_.args_;
    case Op::EqAddr: return os << cmd.eq_addr_.args_;
    case Op::NeChar: return os << cmd.ne_char_.args_;
    case Op::NeInt: return os << cmd.ne_int_.args_;
    case Op::NeReal: return os << cmd.ne_real_.args_;
    case Op::NeFlags: return os << cmd.ne_flags_.args_;
    case Op::NeType: return os << cmd.ne_type_.args_;
    case Op::NeAddr: return os << cmd.ne_addr_.args_;
    case Op::XorBool: return os << cmd.xor_bool_.args_;
    case Op::XorFlags: return os << cmd.xor_flags_.args_;
    case Op::OrFlags: return os << cmd.or_flags_.args_;
    case Op::AndFlags: return os << cmd.and_flags_.args_;
    case Op::CreateStruct: return os;
    case Op::CreateStructField:
      return os << cmd.create_struct_field_.struct_ << " "
                << cmd.create_struct_field_.type_;
    case Op::SetStructFieldName:
      return os << cmd.set_struct_field_name_.struct_ << " "
                << cmd.set_struct_field_name_.name_;
    case Op::FinalizeStruct: return os << cmd.finalize_struct_.reg_;

    case Op::Malloc: return os << cmd.malloc_.arg_;
    case Op::Free: return os << cmd.free_.reg_;
    case Op::Alloca:
      return os << cmd.type->as<type::Pointer>().pointee->to_string();

    case Op::Arrow: return os << cmd.arrow_.args_;
    case Op::Array: return os << cmd.array_.type_;
    case Op::CreateTuple: return os;
    case Op::AppendToTuple:
      return os << cmd.append_to_tuple_.tup_ << " "
                << cmd.append_to_tuple_.arg_;
    case Op::FinalizeTuple:
      return os << cmd.finalize_tuple_.tup_;
    case Op::CreateVariant: return os;
    case Op::AppendToVariant:
      return os << cmd.append_to_variant_.var_ << " "
                << cmd.append_to_variant_.arg_;
    case Op::FinalizeVariant:
      return os << cmd.finalize_variant_.var_;
    case Op::CreateBlockSeq: return os;
    case Op::AppendToBlockSeq:
      return os << cmd.append_to_block_seq_.block_seq_ << " "
                << cmd.append_to_block_seq_.arg_;
    case Op::FinalizeBlockSeq:
      return os << cmd.finalize_block_seq_.block_seq_;
    case Op::VariantType: return os << cmd.variant_type_.reg_;
    case Op::VariantValue: return os << cmd.variant_value_.reg_;
    case Op::PtrIncr: return os << cmd.ptr_incr_.incr_;
    case Op::Field:
      return os << cmd.field_.ptr_ << " " << cmd.field_.struct_type_->to_string() << " "
                << cmd.field_.num_;
    case Op::CondJump:
      return os << cmd.cond_jump_.blocks_[0] << " " << cmd.cond_jump_.blocks_[1];
    case Op::UncondJump: return os << cmd.uncond_jump_.block_;
    case Op::ReturnJump: return os;
    case Op::Call:
      switch (cmd.call_.which_active_) {
        case 0x00: os << cmd.call_.reg_; break;
        case 0x01: os << cmd.call_.fn_; break;
        case 0x02: os << cmd.call_.foreign_fn_.name_; break;
      }
      os << cmd.call_.long_args_->to_string();
      if (cmd.call_.outs_) {
        for (const auto &out : cmd.call_.outs_->outs_) {
          if (out.is_loc_) { os << "*"; }
          os << out.reg_;
        }
      }

      return os;
    case Op::BlockSeqContains: return os << cmd.block_seq_contains_.lit_;

    case Op::CastIntToReal: return os << cmd.cast_int_to_real_.reg_;

    case Op::CastPtr: return os << cmd.cast_ptr_.type_;

    case Op::AddCodeBlock: NOT_YET();
    case Op::Contextualize: NOT_YET();

    case Op::StoreBool:
      return os << cmd.store_bool_.addr_ << " " << cmd.store_bool_.val_;
    case Op::StoreChar:
      return os << cmd.store_char_.addr_ << " " << cmd.store_char_.val_;
    case Op::StoreInt:
      return os << cmd.store_int_.addr_ << " " << cmd.store_int_.val_;
    case Op::StoreReal:
      return os << cmd.store_real_.addr_ << " " << cmd.store_real_.val_;
    case Op::StoreType:
      return os << cmd.store_type_.addr_ << " " << cmd.store_type_.val_;
    case Op::StoreEnum:
      return os << cmd.store_enum_.addr_ << " " << cmd.store_enum_.val_;
    case Op::StoreFlags:
      return os << cmd.store_flags_.addr_ << " " << cmd.store_flags_.val_;
    case Op::StoreAddr:
      return os << cmd.store_addr_.addr_ << " " << cmd.store_addr_.val_;
    case Op::SetReturnBool: return os << cmd.set_return_bool_.val_;
    case Op::SetReturnChar: return os << cmd.set_return_char_.val_;
    case Op::SetReturnInt: return os << cmd.set_return_int_.val_;
    case Op::SetReturnReal: return os << cmd.set_return_real_.val_;
    case Op::SetReturnType: return os << cmd.set_return_type_.val_;
    case Op::SetReturnEnum: return os << cmd.set_return_enum_.val_;
    case Op::SetReturnFlags: return os << cmd.set_return_flags_.val_;
    case Op::SetReturnCharBuf: return os << cmd.set_return_char_buf_.val_;
    case Op::SetReturnAddr: return os << cmd.set_return_addr_.val_;
    case Op::SetReturnFunc: return os << cmd.set_return_func_.val_;
    case Op::SetReturnScope: return os << cmd.set_return_scope_.val_;
    case Op::SetReturnModule: return os << cmd.set_return_module_.val_;
    case Op::SetReturnGeneric: return os << cmd.set_return_generic_.val_;
    case Op::SetReturnBlock: return os << cmd.set_return_block_.val_;
    case Op::PhiBool: return os << cmd.phi_bool_.args_;
    case Op::PhiChar: return os << cmd.phi_char_.args_;
    case Op::PhiInt: return os << cmd.phi_int_.args_;
    case Op::PhiReal: return os << cmd.phi_real_.args_;
    case Op::PhiType: return os << cmd.phi_type_.args_;
    case Op::PhiBlock: return os << cmd.phi_block_.args_;
    case Op::PhiAddr: return os << cmd.phi_addr_.args_;
    case Op::Death: return os;
  }
  UNREACHABLE();
}
}  // namespace IR
