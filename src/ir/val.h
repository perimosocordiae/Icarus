#ifndef ICARUS_IR_VAL_H
#define ICARUS_IR_VAL_H

#include <functional>
#include <iostream>
#include <variant>

#include "../base/owned_ptr.h"
#include "../base/strong_types.h"
#include "../base/types.h"

struct Enum;
struct Type;
struct Pointer;

extern Type *Bool, *Char, *Real, *Int, *Type_, *Void, *String;

namespace AST {
struct CodeBlock;
struct Expression;
struct ScopeLiteral;
struct GenericFunctionLiteral;
struct FunctionLiteral;
} // namespace AST

namespace IR {
DEFINE_STRONG_INT(BlockIndex, i32, -1);
DEFINE_STRONG_INT(ReturnValue, i32, -1);
DEFINE_STRONG_INT(EnumVal, size_t, 0);
DEFINE_STRONG_INT(Register, i32, std::numeric_limits<i32>::lowest());

struct CmdIndex {
  BlockIndex block;
  i32 cmd;
};

inline bool operator==(CmdIndex lhs, CmdIndex rhs) {
  return lhs.block == rhs.block && lhs.cmd == rhs.cmd;
}
inline bool operator<(CmdIndex lhs, CmdIndex rhs) {
  if (lhs.block.value < rhs.block.value) return true;
  if (lhs.block.value > rhs.block.value) return false;
  return lhs.cmd < rhs.cmd;
}

struct Addr {
  enum class Kind : u8 { Null, Global, Stack, Heap } kind;
  union {
    u64 as_global;
    u64 as_stack;
    void *as_heap;
  };

  std::string to_string() const;
};

bool operator==(Addr lhs, Addr rhs);
inline bool operator!=(Addr lhs, Addr rhs) { return !(lhs == rhs); }
inline bool operator<(Addr lhs, Addr rhs) {
  u8 lhs_kind = static_cast<u8>(lhs.kind);
  u8 rhs_kind = static_cast<u8>(rhs.kind);
  if (lhs_kind < rhs_kind) { return true; }
  if (lhs_kind > rhs_kind) { return false; }
  switch (lhs.kind) {
  case Addr::Kind::Null: return false;
  case Addr::Kind::Global: return lhs.as_global < rhs.as_global;
  case Addr::Kind::Stack: return lhs.as_stack < rhs.as_stack;
  case Addr::Kind::Heap: return lhs.as_heap < rhs.as_heap;
  }
  UNREACHABLE();
}
inline bool operator<=(Addr lhs, Addr rhs) { return !(rhs < lhs); }
inline bool operator>(Addr lhs, Addr rhs) { return rhs < lhs; }
inline bool operator>=(Addr lhs, Addr rhs) { return !(lhs < rhs); }

struct Func;
} // namespace IR

DEFINE_STRONG_HASH(IR::ReturnValue);
DEFINE_STRONG_HASH(IR::BlockIndex);
DEFINE_STRONG_HASH(IR::Register);

namespace std {
template <> struct hash<IR::CmdIndex> {
  size_t operator()(const IR::CmdIndex &cmd_index) const noexcept {
    u64 num = (static_cast<u64>(static_cast<u32>(cmd_index.block.value)) << 32);
    num |= static_cast<u32>(cmd_index.cmd);
    return hash<u64>()(num);
  }
};
} // namespace std

namespace IR {
struct Val {
  ::Type *type = nullptr;
  std::variant<Register, ReturnValue, ::IR::Addr, bool, char, double, i32,
               EnumVal, ::Type *, AST::GenericFunctionLiteral *,
               AST::FunctionLiteral *, ::IR::Func *, AST::ScopeLiteral *,
               base::owned_ptr<AST::CodeBlock>, AST::Expression *, BlockIndex,
               std::string>
      value{false};

  static Val Reg(Register r, ::Type *t) { return Val(t, r); }
  static Val Ret(ReturnValue r, ::Type *t) { return Val(t, r); }
  static Val Addr(Addr addr, ::Type *t);
  static Val GlobalAddr(u64 addr, ::Type *t);
  static Val HeapAddr(void *addr, ::Type *t);
  static Val StackAddr(u64 addr, ::Type *t);
  static Val Bool(bool b) { return Val(::Bool, b); }
  static Val Char(char c) { return Val(::Char, c); }
  static Val Real(double r) { return Val(::Real, r); }
  static Val Int(i32 n) { return Val(::Int, n); }
  static Val Enum(const ::Enum *enum_type, size_t integral_val);
  static Val Type(::Type *t) { return Val(::Type_, t); }
  static Val CodeBlock(base::owned_ptr<AST::CodeBlock> block);
  static Val Func(::IR::Func *fn); // TODO deprecate?
  static Val FnLit(AST::FunctionLiteral *fn);
  static Val GenFnLit(AST::GenericFunctionLiteral *fn);
  static Val Block(BlockIndex bi) { return Val(nullptr, bi); }
  static Val Void() { return Val(::Void, false); }
  static Val Null(::Type *t);
  static Val NullPtr();
  static Val StrLit(std::string str) { return Val(::String, std::move(str)); }
  static Val Ref(AST::Expression *expr);
  static Val None() { return Val(); }
  static Val Scope(AST::ScopeLiteral *scope_lit);

  std::string to_string() const;

  Val()                = default;
  ~Val() noexcept      = default;
  Val(const Val &)     = default;
  Val(Val &&) noexcept = default;
  Val &operator=(const Val &) = default;
  Val &operator=(Val &&) noexcept = default;

private:
  template <typename T>
  Val(::Type *t, T &&val) : type(t), value(std::forward<T>(val)) {}
};

inline bool operator==(const Val &lhs, const Val &rhs) {
  return lhs.type == rhs.type && lhs.value == rhs.value;
}
inline bool operator!=(const Val &lhs, const Val &rhs) { return !(lhs == rhs); }
} // namespace IR

#endif // ICARUS_IR_VAL_H
