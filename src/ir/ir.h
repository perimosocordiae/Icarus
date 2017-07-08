#ifndef ICARUS_IR_IR_H
#define ICARUS_IR_IR_H

#include <unordered_map>
#include <string>
#include <vector>
#include <stack>

#include "../base/debug.h"
#include "../base/types.h"

struct Type;
struct Function;
struct Enum;
struct Pointer;

namespace AST {
struct CodeBlock;
struct ScopeLiteral;
} // namespace AST

namespace IR {
struct Func;

struct BlockIndex {
  i32 value = -2;
  bool is_none() { return value == -1; }
};
inline bool operator==(BlockIndex lhs, BlockIndex rhs) {
  return lhs.value == rhs.value;
}

struct RegIndex {
  i32 index = -1;
};

inline bool operator==(RegIndex lhs, RegIndex rhs) {
  return lhs.index == rhs.index;
}
} // namespace IR

namespace std {
template <> struct hash<IR::RegIndex> {
  decltype(auto) operator()(IR::RegIndex r) const noexcept {
    return std::hash<i32>{}(r.index);
  }
};
} // namespace std

namespace IR {
struct Addr {
  enum class Kind : u8 { Null, Global, Stack } kind;
  union {
    u64 as_global;
    u64 as_stack;
  };

  std::string to_string() const;
};
bool operator==(Addr lhs, Addr rhs);

struct Val {
  enum class Kind : u8 { None, Arg, Reg, Const } kind = Kind::None;
  ::Type *type = nullptr;
  union {
    u64 as_arg;
    RegIndex as_reg;
    Addr as_addr;
    bool as_bool;
    char as_char;
    double as_real;
    i64 as_int;  // TODO pick the right type here
    u64 as_uint; // TODO pick the right type here
    size_t as_enum;
    ::Type *as_type;
    ::IR::Func *as_func;
    AST::ScopeLiteral *as_scope;
    AST::CodeBlock *as_code;
    BlockIndex as_block;
    char *as_cstr;
  };

  static Val Arg(Type *t, u64 n);
  static Val Reg(RegIndex r, Type *t);
  static Val Addr(Addr addr, Type *t);
  static Val StackAddr(u64 addr, Type *t);
  static Val GlobalAddr(u64 addr, Type *t);
  static Val Bool(bool b);
  static Val Char(char c);
  static Val Real(double r);
  static Val Int(i64 n);
  static Val Uint(u64 n);
  static Val Enum(::Enum* enum_type, size_t integral_val);
  static Val Type(::Type *t);
  static Val CodeBlock(AST::CodeBlock *block);
  static Val Func(::IR::Func *fn);
  static Val Void();
  static Val Block(BlockIndex bi);
  static Val Null(::Type *t);
  static Val StrLit(const char *cstr);
  static Val None() { return Val(); }
  static Val Scope(AST::ScopeLiteral *scope_lit);

  std::string to_string() const;

  Val() : kind(Kind::Const), as_bool(false) {}
};

inline bool operator==(const Val& lhs, const Val& rhs) {
  return lhs.kind == rhs.kind && lhs.type == rhs.type; // TODO check the right thing.
}
inline bool operator!= (const Val& lhs, const Val& rhs) { return !(lhs == rhs); }

enum class Op : char {
  Trunc, Extend,
  Neg, // ! for bool, - for numeric types
  Add, Sub, Mul, Div, Mod, // numeric types only
  Lt, Le, Eq, Ne, Gt, Ge, // numeric types only
  And, Or, Xor, // bool only
  Print, Malloc, Free,
  Load, Store,
  ArrayLength, ArrayData,
  PtrIncr,
  Phi, Field, Access,
  Call, Cast,
  Nop, SetReturn,
  Arrow, Array, Ptr,
  Alloca,
};

struct Block;
struct Cmd;

struct Stack {
  Stack() = delete;
  Stack(size_t cap) : capacity_(cap), stack_(malloc(capacity_)) {}
  Stack(const Stack&) = delete;
  Stack(Stack&& other) {
    free(stack_);
    stack_ = other.stack_;
    other.stack_ = nullptr;
    other.capacity_ = other.size_ = 0;
  }
  ~Stack() { free(stack_); }

  template <typename T> T Load(size_t index) {
    ASSERT((index & (alignof(T) - 1)) == 0, "Alignment error");
    return *reinterpret_cast<T *>(this->location(index));
  }

  template <typename T> void Store(T val, size_t index) {
    *reinterpret_cast<T *>(this->location(index)) = val;
  }

  IR::Val Push(Pointer *ptr);

  size_t capacity_ = 0;
  size_t size_     = 0;
  void *stack_     = nullptr;

private:
  void *location(size_t index) {
    ASSERT(index < capacity_,
           std::to_string(index) + " !< " + std::to_string(capacity_));
    return reinterpret_cast<void *>(reinterpret_cast<char *>(stack_) + index);
  }
};

struct ExecContext {
  ExecContext();

  struct Frame {
    Frame() = delete;
    Frame(Func *fn, std::vector<Val> arguments);

    Func *fn_ = nullptr;
    BlockIndex current_;
    BlockIndex prev_;

    // Indexed first by block then by instruction number
    std::vector<Val> regs_ = {};
    std::vector<Val> args_ = {};
    std::vector<Val> rets_ = {};
  };
  std::stack<Frame> call_stack;

  BlockIndex ExecuteBlock();
  Val ExecuteCmd(const Cmd& cmd);
  void Resolve(Val *v) const;

  Val reg(RegIndex r) const { return call_stack.top().regs_[r.index]; }
  Val &reg(RegIndex r) { return call_stack.top().regs_[r.index]; }
  Val arg(u64 n) const { return call_stack.top().args_[n]; }

  Stack stack_;
};

struct Cmd {
  Cmd() : op_code(Op::Nop), result(IR::Val::None()) {}
  Cmd(Type *t, Op op, std::vector<Val> args);
  std::vector<Val> args;
  Op op_code;

  Val result; // Will always be of Kind::Reg.

  void dump(size_t indent) const;
};

Val Neg(Val v);
Val Trunc(Val v);
Val Extend(Val v);
Val Add(Val v1, Val v2);
Val Sub(Val v1, Val v2);
Val Mul(Val v1, Val v2);
Val Div(Val v1, Val v2);
Val Mod(Val v1, Val v2);
Val Lt(Val v1, Val v2);
Val Le(Val v1, Val v2);
Val Eq(Val v1, Val v2);
Val Ne(Val v1, Val v2);
Val Ge(Val v1, Val v2);
Val Gt(Val v1, Val v2);
Val And(Val v1, Val v2);
Val Or(Val v1, Val v2);
Val Xor(Val v1, Val v2);
Val Print(Val v);
Val Cast(Val result_type, Val val);
Val Call(Val fn, std::vector<Val> vals);
Val SetReturn(size_t n, Val v);
Val Access(Val index, Val val);
Val Load(Val v);
Val Store(Val val, Val loc);
Val ArrayLength(Val v);
Val ArrayData(Val v);
Val PtrIncr(Val v1, Val v2);
Val Malloc(Type *t, Val v);
Val Free(Val v);
Val Phi(Type *t);
Val Field(Val v, size_t n);
Val Arrow(Val v1, Val v2);
Val Array(Val v1, Val v2);
Val Ptr(Val v1);
Val Alloca(Type *t);

struct Jump {
  static void Unconditional(BlockIndex index);
  static void Conditional(Val cond, BlockIndex true_index,
                          BlockIndex false_index);
  static void Return();

  void dump(size_t indent) const;

  Jump() {}
  ~Jump() {}
  struct CondData {
    Val cond;
    BlockIndex true_block;
    BlockIndex false_block;
  };

  enum class Type : u8 { None, Uncond, Cond, Ret } type = Type::None;
  union {
    BlockIndex block_index; // for unconditional jump
    CondData cond_data; // value and block indices to jump for conditional jump.
  };
};

struct Block {
  static BlockIndex Current;
  Block() = delete;
  Block(Func* fn) : fn_(fn) {}

  void dump(size_t indent) const;

  Func *fn_; // Containing function
  std::vector<Cmd> cmds_;
  Jump jmp_;
};

struct Func {
  static Func *Current;
  Func(::Function *fn_type) : type(fn_type), blocks_(2, Block(this)) {}

  void dump() const;

  Cmd &Command(RegIndex reg);
  void SetArgs(RegIndex reg, std::vector<IR::Val> args);

  static BlockIndex AddBlock() {

    BlockIndex index;
    index.value =
        static_cast<decltype(index.value)>(IR::Func::Current->blocks_.size());
    IR::Func::Current->blocks_.emplace_back(IR::Func::Current);
    return index;
  }

  BlockIndex entry() const {
    BlockIndex index;
    index.value = 0;
    return index;
  }

  BlockIndex exit() const {
    BlockIndex index;
    index.value = 1;
    return index;
  }

  std::vector<Val> Execute(std::vector<Val> args, ExecContext *ctx);

  ::Function *type = nullptr;
  i32 num_cmds_ = 0;
  std::string name;
  std::vector<Block> blocks_;
  std::unordered_map<RegIndex, std::pair<BlockIndex, int>> reg_map_;
};

struct FuncResetter {
  FuncResetter(Func *fn)
      : old_fn_(IR::Func::Current), old_block_(IR::Block::Current) {
    IR::Func::Current = fn;
  }
  ~FuncResetter() {
    IR::Func::Current = old_fn_;
    IR::Block::Current = old_block_;
  }

  Func *old_fn_;
  BlockIndex old_block_;
  bool cond_ = true;
};

#define CURRENT_FUNC(fn)                                                       \
  for (auto resetter = ::IR::FuncResetter(fn); resetter.cond_;                 \
       resetter.cond_ = false)
} // namespace IR

#endif // ICARUS_IR_IR_H