#include "Type/Type.h"
#include <cstdio>
#include "IR/Stack.h"

#include <ncurses.h>

extern const char *GetGlobalStringNumbered(size_t index);
extern std::stack<Scope *> ScopeStack;
extern IR::Value GetInitialGlobal(size_t global_addr);
extern void AddInitialGlobal(size_t global_addr, IR::Value val);
extern std::string Escape(char c);

namespace IR {
extern std::string OpCodeString(Op op_code);

void RefreshDisplay(const StackFrame &frame, LocalStack *local_stack) {
  clear();
  int height = 1 + ((int)local_stack->used) / 4;
  for (int i = 0; i < height; ++i) {
    mvprintw(i, 0, "%2d. [ %2x %2x %2x %2x ]", 4 * i,
             local_stack->allocs[4 * i] & 0xff,
             local_stack->allocs[4 * i + 1] & 0xff,
             local_stack->allocs[4 * i + 2] & 0xff,
             local_stack->allocs[4 * i + 3] & 0xff);
  }
  mvprintw(height, 0, "    [-------------]");

  for (size_t i = 0; i < frame.reg.size(); ++i) {
    switch (frame.reg[i].flag) {
    case ValType::Val: {
      mvprintw((int)i, 100, "%2d.> %8s", i,
               frame.reg[i].as_val->to_string().c_str());
    } break;
    case ValType::HeapAddr: {
      mvprintw((int)i, 100, "%2d.> %8lu (heap addr)", i,
               frame.reg[i].as_heap_addr);
    } break;
    case ValType::Loc: {
      mvprintw((int)i, 100, "%2d.> %8s", i,
               frame.reg[i].as_loc->to_string().c_str());
    } break;
    case ValType::None: {
      mvprintw((int)i, 100, "%2d.> --", i);
    } break;
    default: { mvprintw((int)i, 100, "%2d.> ........", i); } break;
    }
  }

  int row = 0;
  mvprintw(0, 30, "%s", frame.curr_block->block_name);
  start_color();
  init_pair(1, COLOR_BLACK, COLOR_WHITE);
  for (const auto &cmd : frame.curr_block->cmds) {
    std::stringstream ss;
    ss << *cmd.result.type;
    if (cmd.result.type == Void) {
      ss << "\t\t  ";
    } else {
      ss << " %" << cmd.result.reg << "\t= ";
    }
    ss << OpCodeString(cmd.op_code) << " " << cmd.args[0];
    for (size_t i = 1; i < cmd.args.size(); ++i) { ss << ", " << cmd.args[i]; }

    auto output = ss.str();

    if (row == (int)frame.inst_ptr) {
      attron(COLOR_PAIR(1));
      mvprintw(++row, 34, "%s", output.c_str());
      attroff(COLOR_PAIR(1));
    } else {
      mvprintw(++row, 34, "%s", output.c_str());
    }
  }

  frame.curr_block->exit->ShowExit(row);

  mvprintw(39, 2, "Stack size:     %ld", local_stack->used);
  mvprintw(40, 2, "Stack capacity: %ld", local_stack->capacity);
  mvprintw(41, 2, "Frame offset:   %ld", frame.offset);
  mvprintw(42, 2, "Instr. Pointer: %ld", frame.inst_ptr);

  getch();
  refresh();
}

void Exit::Unconditional::ShowExit(int &row) {
  mvprintw(++row, 34, "jmp %s", block->block_name);
}

void Exit::Conditional::ShowExit(int &row) {
  std::stringstream ss;
  ss << cond;
  mvprintw(++row, 34, "br %s [T: %s] [F: %s]", ss.str().c_str(),
           true_block->block_name, false_block->block_name);
}

void Exit::Return::ShowExit(int &row) {
  std::stringstream ss;
  ss << ret_val;
  mvprintw(++row, 34, "ret %s", ss.str().c_str());
}

void Exit::ReturnVoid::ShowExit(int &row) { mvprintw(++row, 34, "ret"); }

void Exit::Switch::ShowExit(int &row) {
  std::stringstream ss;
  ss << cond;
  mvprintw(++row, 34, "switch (%llu) %s", table.size(), ss.str().c_str());
  for (auto entry : table) {
    std::stringstream ss;
    ss << entry.first;
    mvprintw(++row, 36, "[%s => %s]", ss.str().c_str(),
             entry.second->block_name);
  }
}

static Value ResolveValue(const StackFrame &frame, const Value &v) {
  if (v.flag == ValType::Loc) {
    if (v.as_loc->is_reg()) { return frame.reg[v.as_loc->GetReg()]; }
    if (v.as_loc->is_arg()) { return frame.args[v.as_loc->GetArg()]; }
  }
  if (v.flag == ValType::Loc && v.as_loc->is_frame_addr()) {
    return Value::StackAddr(v.as_loc->GetFrameAddr() + frame.offset);
  }
  return v;
}

void Cmd::Execute(StackFrame& frame) {
  std::vector<Value> cmd_inputs;
  for (size_t i = 0; i < args.size(); ++i) {
    cmd_inputs.push_back(ResolveValue(frame, args[i]));
  }

  switch (op_code) {
  case Op::BNot: {
    frame.reg[result.reg] = Value::Bool(!cmd_inputs[0].as_val->GetBool());
  } break;
  case Op::INeg: {
    frame.reg[result.reg] = Value::Int(-cmd_inputs[0].as_val->GetInt());
  } break;
  case Op::FNeg: {
    frame.reg[result.reg] = Value::Real(-cmd_inputs[0].as_val->GetReal());
  } break;
  case Op::Call: {
    // Need to load the right address.
    switch (cmd_inputs[0].flag) {
    case ValType::HeapAddr:
      cmd_inputs[0] = IR::Value::Func(*(Func **)cmd_inputs[0].as_heap_addr);
      break;
    case ValType::Loc:
      if (cmd_inputs[0].as_loc->is_stack_addr()) {
        cmd_inputs[0] = IR::Value::Func(
            *(Func **)(frame.stack->allocs + cmd_inputs[0].as_loc->GetStackAddr()));
      } else {
        UNREACHABLE;
      }
      break;
    case ValType::Val: break;
    default: UNREACHABLE;
    }
    assert(cmd_inputs[0].flag == ValType::Val &&
           cmd_inputs[0].as_val->is_function());

    std::vector<Value> call_args;
    for (size_t i = 1; i < cmd_inputs.size(); ++i) {
      call_args.push_back(cmd_inputs[i]);
    }

    auto call_result =
        cmd_inputs[0].as_val->GetFunc()->Call(frame.stack, call_args);
    if (result.type != Void) { frame.reg[result.reg] = call_result; }
  } break;
  case Op::Cast: {
    assert(cmd_inputs[0].flag == ValType::Val && cmd_inputs[0].as_val->is_type());
    assert(cmd_inputs[1].flag == ValType::Val && cmd_inputs[1].as_val->is_type());
    if (cmd_inputs[0].as_val->GetType() == Char) {
      auto c = cmd_inputs[2].as_val->GetChar();
      if (cmd_inputs[1].as_val->GetType() == Char) {
        frame.reg[result.reg] = Value::Char(c);
      } else if (cmd_inputs[1].as_val->GetType() == Uint) {
        frame.reg[result.reg] = Value::Uint((size_t)c);
      } else {
        NOT_YET;
      }
    } else if (cmd_inputs[0].as_val->GetType() == Int) {
      auto i = cmd_inputs[2].as_val->GetInt();
      if (cmd_inputs[1].as_val->GetType() == Char) {
        frame.reg[result.reg] = Value::Char((char)i);
      } else if (cmd_inputs[1].as_val->GetType() == Int) {
        frame.reg[result.reg] = Value::Int(i);
      } else if (cmd_inputs[1].as_val->GetType() == Real) {
        frame.reg[result.reg] = Value::Real((double)i);
      } else if (cmd_inputs[1].as_val->GetType() == Uint) {
        frame.reg[result.reg] = Value::Uint((size_t)i);
      } else {
        NOT_YET;
      }
    } else if (cmd_inputs[0].as_val->GetType() == Uint) {
      auto u = cmd_inputs[2].as_val->GetUint();
      if (cmd_inputs[1].as_val->GetType() == Char) {
        frame.reg[result.reg] = Value::Char((char)u);
      } else if (cmd_inputs[1].as_val->GetType() == Int) {
        frame.reg[result.reg] = Value::Int((long)u);
      } else if (cmd_inputs[1].as_val->GetType() == Real) {
        frame.reg[result.reg] = Value::Real((double)u);
      } else if (cmd_inputs[1].as_val->GetType() == Uint) {
        frame.reg[result.reg] = Value::Uint(u);
      } else {
        NOT_YET;
      }
    } else {
      NOT_YET;
    }
  } break;
  case Op::Malloc: {
    auto ptr = malloc(cmd_inputs[0].as_val->GetUint());
    if (!ptr) {
      fprintf(stderr, "malloc failed to allocate %lu byte(s).\n",
              cmd_inputs[1].as_val->GetUint());
      std::exit(-1);
    }
    frame.reg[result.reg] = Value::HeapAddr(ptr);
  } break;
  case Op::Free: {
    free(cmd_inputs[0].as_heap_addr);
  } break;
  case Op::Memcpy: {
    void *dest;
    switch (cmd_inputs[0].flag) {
    case ValType::HeapAddr: dest = cmd_inputs[0].as_heap_addr; break;
    default: UNREACHABLE;
    }

    const void *source;
    switch (cmd_inputs[1].flag) {
    case ValType::GlobalCStr:
      source = GetGlobalStringNumbered(cmd_inputs[1].as_global_cstr);
      break;
    default: UNREACHABLE;
    }

    memcpy(dest, source, cmd_inputs[2].as_val->GetUint());
  } break;
  case Op::Load: {
    if (result.type->is_enum()) { result.type = ((Enum *)result.type)->ProxyType(); }
    assert(
        (result.type->is_primitive() || result.type->is_pointer() ||
         result.type->is_function() || result.type->is_scope_type()) &&
        "Non-primitive/pointer/enum local variables are not yet implemented");
    if (cmd_inputs[0].flag == ValType::Loc &&
        cmd_inputs[0].as_loc->is_stack_addr()) {
      size_t offset = cmd_inputs[0].as_loc->GetStackAddr();

#define DO_LOAD(StoredType, type_name, stored_type)                            \
  if (result.type->is_##type_name()) {                                         \
    frame.reg[result.reg] =                                                    \
        Value::StoredType(*(stored_type *)(frame.stack->allocs + offset));     \
    break;                                                                     \
  }

      DO_LOAD(Bool, bool, bool);
      DO_LOAD(Char, char, char);
      DO_LOAD(Int,  int,  long);
      DO_LOAD(Real, real, double);
      DO_LOAD(U16,  u16,  uint16_t);
      DO_LOAD(U32,  u32,  uint32_t);
      DO_LOAD(Uint, uint, size_t);
      DO_LOAD(Type, type, Type *);

#undef DO_LOAD
      assert(result.type->is_pointer() || result.type->is_function() ||
             result.type->is_scope_type());
      if (result.type->is_pointer()) {
        // TODO how do we determine if it's heap or stack?
        auto ptr_as_uint = *(size_t *)(frame.stack->allocs + offset);
        if (ptr_as_uint > 0xffff) { // NOTE hack to guess if it's a heap address
          frame.reg[result.reg] = Value::HeapAddr((void *)ptr_as_uint);
        } else {
          frame.reg[result.reg] = Value::StackAddr(ptr_as_uint);
        }
      } else if (result.type->is_function()) {
        frame.reg[result.reg] =
            Value::Func(*(Func **)(frame.stack->allocs + offset));
      } else {
        frame.reg[result.reg] =
            Value::Scope(*(AST::ScopeLiteral **)(frame.stack->allocs + offset));
      }
    } else if (cmd_inputs[0].flag == ValType::HeapAddr) {

#define DO_LOAD(StoredType, type_name, stored_type)                            \
  if (result.type->is_##type_name()) {                                          \
    frame.reg[result.reg] =                                                    \
        Value::StoredType(*(stored_type *)(cmd_inputs[0].as_heap_addr));       \
    break;                                                                     \
  }

      DO_LOAD(Bool, bool, bool);
      DO_LOAD(Char, char, char);
      DO_LOAD(Int,  int,  long);
      DO_LOAD(Real, real, double);
      DO_LOAD(U16,  u16,  uint16_t);
      DO_LOAD(U32,  u32,  uint32_t);
      DO_LOAD(Uint, uint, size_t);
      DO_LOAD(Type, type, Type *);

#undef DO_LOAD
      assert(result.type->is_pointer());
      // TODO how do we determine if it's heap or stack?
      auto ptr_as_uint = *(size_t *)(cmd_inputs[0].as_heap_addr);
      if (ptr_as_uint > 0xffff) { // NOTE hack to guess if it's a heap address
        frame.reg[result.reg] = Value::HeapAddr((void *)ptr_as_uint);
      } else {
        frame.reg[result.reg] = Value::StackAddr(ptr_as_uint);
      }

    } else if (cmd_inputs[0].flag == ValType::Loc &&
               cmd_inputs[0].as_loc->is_global_addr()) {
      frame.reg[result.reg] = GetInitialGlobal(cmd_inputs[0].as_loc->GetGlobalAddr());

    } else if (cmd_inputs[0].flag == ValType::Val &&
               cmd_inputs[0].as_val->is_function()) {
      frame.reg[result.reg] = cmd_inputs[0];

    } else {
      frame.curr_block->dump();
      UNREACHABLE;
    }
  } break;

  case Op::Store: {
    assert(cmd_inputs[0].flag == ValType::Val &&
           cmd_inputs[0].as_val->is_type() &&
           (cmd_inputs[0].as_val->GetType()->is_primitive() ||
            cmd_inputs[0].as_val->GetType()->is_pointer() ||
            cmd_inputs[0].as_val->GetType()->is_scope_type() ||
            cmd_inputs[0].as_val->GetType()->is_function()));
    assert((cmd_inputs[2].flag == ValType::Loc &&
            cmd_inputs[2].as_loc->is_stack_addr()) ||
           cmd_inputs[2].flag == ValType::HeapAddr ||
           (cmd_inputs[2].flag == ValType::Loc &&
            cmd_inputs[2].as_loc->is_global_addr()));

    if (cmd_inputs[2].flag == ValType::Loc &&
        cmd_inputs[2].as_loc->is_stack_addr()) {
      size_t offset = cmd_inputs[2].as_loc->GetStackAddr();

#define VAL_MACRO(TypeName, type_name, cpp_type)                               \
  if (cmd_inputs[0].as_val->GetType()->is_##type_name()) {                     \
    auto ptr = (cpp_type *)(frame.stack->allocs + offset);                     \
    *ptr     = cmd_inputs[1].as_val->Get##TypeName();                          \
    break;                                                                     \
  }
#include "../config/val.conf"
#undef VAL_MACRO

      if (cmd_inputs[0].as_val->GetType() == String) {
        auto ptr = (void **)(frame.stack->allocs + offset);
        switch (cmd_inputs[1].flag) {
        case ValType::GlobalCStr:
          *ptr = const_cast<char *>(
              GetGlobalStringNumbered(cmd_inputs[1].as_global_cstr));
          break;
        case ValType::HeapAddr: *ptr = cmd_inputs[1].as_heap_addr; break;
        default: NOT_YET;
        }
        break;

      } else if (cmd_inputs[0].as_val->GetType()->is_pointer()) {
        auto ptr = (void **)(frame.stack->allocs + offset);
        switch (cmd_inputs[1].flag) {
        case ValType::HeapAddr: *ptr = cmd_inputs[1].as_heap_addr; break;
        default: frame.curr_block->dump(); NOT_YET;
        }
        break;

      } else if (cmd_inputs[0].as_val->GetType()->is_function()) {
        auto ptr = (void **)(frame.stack->allocs + offset);
        switch (cmd_inputs[1].flag) {
        case ValType::Val:
          if (cmd_inputs[1].as_val->is_function()) {
            *ptr = cmd_inputs[1].as_val->GetFunc();
          } else if (cmd_inputs[1].as_val->is_null()) {
            *ptr = nullptr;
          } else {
            NOT_YET;
          }
        case ValType::HeapAddr: *ptr = cmd_inputs[1].as_heap_addr; break;
        default: dump(0); NOT_YET;
        }
        break;
      }

      if (cmd_inputs[0].as_val->GetType() == Void) {
        auto ptr = (void *)(frame.stack->allocs + offset);
        *(size_t *)ptr = cmd_inputs[1].as_loc->GetStackAddr();
        break;
      }
    } else if (cmd_inputs[2].flag == ValType::HeapAddr) {

#define VAL_MACRO(TypeName, type_name, cpp_type)                               \
  if (cmd_inputs[0].as_val->GetType()->is_##type_name()) {                     \
    auto ptr = (cpp_type *)(cmd_inputs[2].as_heap_addr);                       \
    *ptr     = cmd_inputs[1].as_val->Get##TypeName();                          \
    break;                                                                     \
  }
#include "../config/val.conf"
#undef VAL_MACRO

      if (cmd_inputs[0].as_val->GetType() == String) {
        NOT_YET;
        break;

      } else if (cmd_inputs[0].as_val->GetType()->is_function()) {
        NOT_YET;
        break;

      } else if (cmd_inputs[0].as_val->GetType()->is_pointer()) {
        NOT_YET;
        break;

      } else if (cmd_inputs[0].as_val->GetType() == Void) {
        NOT_YET;
        break;
      }
    } else {
      AddInitialGlobal(cmd_inputs[2].as_loc->GetGlobalAddr(), cmd_inputs[1]);
      break;
    }

    std::cerr << *cmd_inputs[0].as_val->GetType() << std::endl;
    UNREACHABLE;
  } break;
  case Op::Field: {
    auto struct_type = (Struct *)cmd_inputs[0].as_val->GetType();

    // TODO what if it's heap-allocated?
    assert(cmd_inputs[1].flag == ValType::Loc &&
           cmd_inputs[1].as_loc->is_stack_addr());
    assert(cmd_inputs[2].flag == ValType::Val);

    auto ptr = cmd_inputs[1].as_loc->GetStackAddr();

    size_t field_index = cmd_inputs[2].as_val->GetUint();

    // field_index + 1 is correct because it simplifies the offset computation
    // to have the zero present.
    ptr += struct_type->field_offsets AT(field_index + 1);
    frame.reg[result.reg] = Value::StackAddr(ptr);
  } break;
  case Op::PtrIncr: {
    switch (cmd_inputs[1].flag) {
    case ValType::Loc: {
      if (cmd_inputs[1].as_loc->is_stack_addr()) {
        assert(cmd_inputs[0].as_val->GetType()->is_pointer());
        // TODO space in array or bytes()?
        auto pointee_size = ((Pointer *)cmd_inputs[0].as_val->GetType())
                                ->pointee->SpaceInArray();
        frame.reg[result.reg] =
            Value::StackAddr(cmd_inputs[1].as_loc->GetStackAddr() +
                             cmd_inputs[2].as_val->GetUint() * pointee_size);
      }
      UNREACHABLE;
    } break;
    case ValType::HeapAddr: {
      assert(cmd_inputs[0].flag == ValType::Val &&
             cmd_inputs[0].as_val->is_type());
      assert(cmd_inputs[0].as_val->GetType()->is_pointer());
      // TODO space in array or bytes()?
      auto pointee_size =
          ((Pointer *)cmd_inputs[0].as_val->GetType())->pointee->SpaceInArray();
      frame.reg[result.reg] =
          Value::HeapAddr((char *)cmd_inputs[1].as_heap_addr +
                          cmd_inputs[2].as_val->GetUint() * pointee_size);
    } break;
    default:
      const_cast<Func *>(frame.func)->dump();
      std::cerr << cmd_inputs[1] << '\n';
      UNREACHABLE;
    }

  } break;
  case Op::Access: {
    switch (cmd_inputs[2].flag) {
    case ValType::Loc:
      if (cmd_inputs[2].as_loc->is_stack_addr()) {
        frame.reg[result.reg] = Value::StackAddr(
            cmd_inputs[2].as_loc->GetStackAddr() +
            cmd_inputs[1].as_val->GetUint() *
                cmd_inputs[0].as_val->GetType()->SpaceInArray());
      } else {
        UNREACHABLE;
      }
      break;
    case ValType::HeapAddr:
      frame.reg[result.reg] =
          Value::HeapAddr((char *)(cmd_inputs[2].as_heap_addr) +
                          cmd_inputs[1].as_val->GetUint() *
                              cmd_inputs[0].as_val->GetType()->SpaceInArray());
      break;
    default: std::cerr << cmd_inputs[2] << '\n'; UNREACHABLE;
    }
  } break;
  case Op::Phi: {
    assert((cmd_inputs.size() & 1u) == 0u);
    for (size_t i = 0; i < cmd_inputs.size(); i += 2) {
      if (frame.prev_block == cmd_inputs[i].as_block) {
        frame.reg[result.reg] = cmd_inputs[i + 1];
        goto exit_successfully;
      }
    }
    std::cerr << frame.prev_block << '\n';
    assert(false && "No selection made from phi block");
  exit_successfully:;
  } break;
  case Op::CAdd: {
    frame.reg[result.reg] =
        Value::Char((char)(cmd_inputs[0].as_val->GetChar() +
                           cmd_inputs[1].as_val->GetChar()));
  } break;
  case Op::IAdd: {
    frame.reg[result.reg] = Value::Int(cmd_inputs[0].as_val->GetInt() +
                                       cmd_inputs[1].as_val->GetInt());
  } break;
  case Op::UAdd: {
    frame.reg[result.reg] = Value::Uint(cmd_inputs[0].as_val->GetUint() +
                                        cmd_inputs[1].as_val->GetUint());
  } break;
  case Op::FAdd: {
    frame.reg[result.reg] = Value::Real(cmd_inputs[0].as_val->GetReal() +
                                        cmd_inputs[1].as_val->GetReal());
  } break;
  case Op::ISub: {
    frame.reg[result.reg] = Value::Int(cmd_inputs[0].as_val->GetInt() -
                                       cmd_inputs[1].as_val->GetInt());
  } break;
  case Op::USub: {
    frame.reg[result.reg] = Value::Uint(cmd_inputs[0].as_val->GetUint() -
                                        cmd_inputs[1].as_val->GetUint());
  } break;
  case Op::FSub: {
    frame.reg[result.reg] = Value::Real(cmd_inputs[0].as_val->GetReal() -
                                        cmd_inputs[1].as_val->GetReal());
  } break;
  case Op::IMul: {
    frame.reg[result.reg] = Value::Int(cmd_inputs[0].as_val->GetInt() *
                                       cmd_inputs[1].as_val->GetInt());
  } break;
  case Op::UMul: {
    frame.reg[result.reg] = Value::Uint(cmd_inputs[0].as_val->GetUint() *
                                        cmd_inputs[1].as_val->GetUint());
  } break;
  case Op::FMul: {
    frame.reg[result.reg] = Value::Real(cmd_inputs[0].as_val->GetReal() *
                                        cmd_inputs[1].as_val->GetReal());
  } break;
  case Op::IDiv: {
    frame.reg[result.reg] = Value::Int(cmd_inputs[0].as_val->GetInt() /
                                       cmd_inputs[1].as_val->GetInt());
  } break;
  case Op::UDiv: {
    frame.reg[result.reg] = Value::Uint(cmd_inputs[0].as_val->GetUint() /
                                        cmd_inputs[1].as_val->GetUint());
  } break;
  case Op::FDiv: {
    frame.reg[result.reg] = Value::Real(cmd_inputs[0].as_val->GetReal() /
                                        cmd_inputs[1].as_val->GetReal());
  } break;
  case Op::IMod: {
    frame.reg[result.reg] = Value::Int(cmd_inputs[0].as_val->GetInt() %
                                       cmd_inputs[1].as_val->GetInt());
  } break;
  case Op::UMod: {
    frame.reg[result.reg] = Value::Uint(cmd_inputs[0].as_val->GetUint() %
                                        cmd_inputs[1].as_val->GetUint());
  } break;
  case Op::FMod: {
    frame.reg[result.reg] = Value::Real(
        fmod(cmd_inputs[0].as_val->GetReal(), cmd_inputs[1].as_val->GetReal()));
  } break;
  case Op::BOr: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetBool() ||
                                        cmd_inputs[1].as_val->GetBool());
  } break;
  case Op::BXor: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetBool() !=
                                        cmd_inputs[1].as_val->GetBool());
  } break;
  case Op::ILT: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetInt() <
                                       cmd_inputs[1].as_val->GetInt());
  } break;
  case Op::ULT: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetUint() <
                                        cmd_inputs[1].as_val->GetUint());
  } break;
  case Op::FLT: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetReal() <
                                        cmd_inputs[1].as_val->GetReal());
  } break;
  case Op::ILE: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetInt() <=
                                       cmd_inputs[1].as_val->GetInt());
  } break;
  case Op::ULE: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetUint() <=
                                        cmd_inputs[1].as_val->GetUint());
  } break;
  case Op::FLE: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetReal() <=
                                        cmd_inputs[1].as_val->GetReal());
  } break;
  case Op::IEQ: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetInt() ==
                                       cmd_inputs[1].as_val->GetInt());
  } break;
  case Op::UEQ: {
    frame.reg[result.reg] =
        Value::Bool(cmd_inputs[0].as_val->GetUint() == cmd_inputs[1].as_val->GetUint());
  } break;
  case Op::FEQ: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetChar() ==
                                        cmd_inputs[1].as_val->GetChar());
  } break;
  case Op::PtrEQ: {
    // TODO Fix the types here
    frame.reg[result.reg] =
        Value::Bool(cmd_inputs[0].flag == cmd_inputs[1].flag &&
                    cmd_inputs[0].as_heap_addr == cmd_inputs[1].as_heap_addr);
  } break;
  case Op::BEQ: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetBool() ==
                                        cmd_inputs[1].as_val->GetBool());
  } break;
  case Op::CEQ: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetChar() ==
                                        cmd_inputs[1].as_val->GetChar());
  } break;
  case Op::TEQ: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetType() ==
                                        cmd_inputs[1].as_val->GetType());
  } break;
  case Op::FnEQ: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetFunc() ==
                                        cmd_inputs[1].as_val->GetFunc());
  } break;
  case Op::INE: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetInt() !=
                                       cmd_inputs[1].as_val->GetInt());
  } break;
  case Op::UNE: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetUint() !=
                                        cmd_inputs[1].as_val->GetUint());
  } break;
  case Op::FNE: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetChar() !=
                                        cmd_inputs[1].as_val->GetChar());
  } break;
  case Op::BNE: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetBool() !=
                                        cmd_inputs[1].as_val->GetBool());
  } break;
  case Op::CNE: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetChar() !=
                                        cmd_inputs[1].as_val->GetChar());
  } break;
  case Op::TNE: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetType() !=
                                        cmd_inputs[1].as_val->GetType());
  } break;
  case Op::FnNE: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetFunc() !=
                                        cmd_inputs[1].as_val->GetFunc());
  } break;
  case Op::IGE: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetInt() >=
                                       cmd_inputs[1].as_val->GetInt());
  } break;
  case Op::UGE: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetUint() >=
                                        cmd_inputs[1].as_val->GetUint());
  } break;
  case Op::FGE: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetReal() >=
                                        cmd_inputs[1].as_val->GetReal());
  } break;
  case Op::CGT: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetChar() >
                                        cmd_inputs[1].as_val->GetChar());
  } break;
  case Op::IGT: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetInt() >
                                       cmd_inputs[1].as_val->GetInt());
  } break;
  case Op::UGT: {
    frame.reg[result.reg] =
        Value::Bool(cmd_inputs[0].as_val->GetUint() > cmd_inputs[1].as_val->GetUint());
  } break;
  case Op::FGT: {
    frame.reg[result.reg] = Value::Bool(cmd_inputs[0].as_val->GetReal() >
                                        cmd_inputs[1].as_val->GetReal());
  } break;
  case Op::NOp: break;
  case Op::Print: {
    if (cmd_inputs[0].as_val->GetType() == Bool) {
      fprintf(stderr, cmd_inputs[1].as_val->GetBool() ? "true" : "false");

    } else if (cmd_inputs[0].as_val->GetType() == Char) {
      fprintf(stderr, "%c", cmd_inputs[1].as_val->GetChar());

    } else if (cmd_inputs[0].as_val->GetType() == Int) {
      fprintf(stderr, "%ld", cmd_inputs[1].as_val->GetInt());

    } else if (cmd_inputs[0].as_val->GetType() == Real) {
      fprintf(stderr, "%lf", cmd_inputs[1].as_val->GetReal());

    } else if (cmd_inputs[0].as_val->GetType() == Uint) {
      fprintf(stderr, "%lu", cmd_inputs[1].as_val->GetUint());

    } else if (cmd_inputs[0].as_val->GetType() == Type_) {
      fprintf(stderr, "%s", cmd_inputs[1].as_val->GetType()->to_string().c_str());

    } else if(cmd_inputs[0].as_val->GetType() == String) {
      if (cmd_inputs[1].flag == ValType::GlobalCStr) {
        fprintf(stderr, "%s",
                GetGlobalStringNumbered(cmd_inputs[1].as_global_cstr));
      }

    } else if (cmd_inputs[0].as_val->GetType()->is_enum()) {
      auto enum_type = (Enum *)cmd_inputs[0].as_val->GetType();
      fprintf(stderr, "%s.%s", enum_type->to_string().c_str(),
              enum_type->members[cmd_inputs[1].as_val->GetUint()].c_str());

    } else if (cmd_inputs[0].as_val->GetType()->is_pointer()) {
      fprintf(stderr, "0x%lx", cmd_inputs[0].as_val->GetUint());

    } else {
      UNREACHABLE;
    }
  } break;
  case Op::TC_Tup: {
    std::vector<Type *> type_vec;
    for (auto v : cmd_inputs) {
      assert(v.flag == ValType::Val && v.as_val->is_type());
      type_vec.push_back(v.as_val->GetType());
    }
    frame.reg[result.reg] = Value::Type(Tup(type_vec));
  } break;
  case Op::TC_Ptr: {
    frame.reg[result.reg] = Value::Type(Ptr(cmd_inputs[0].as_val->GetType()));
  } break;
  case Op::TC_Arrow: {
    frame.reg[result.reg] = Value::Type(::Func(
        cmd_inputs[0].as_val->GetType(), cmd_inputs[1].as_val->GetType()));
  } break;
  case Op::TC_Arr1: {
    frame.reg[result.reg] = Value::Type(Arr(cmd_inputs[0].as_val->GetType()));
  } break;
  case Op::TC_Arr2: {
    if (cmd_inputs[0].as_val->is_int()) {
      frame.reg[result.reg] =
          Value::Type(Arr(cmd_inputs[1].as_val->GetType(),
                          (size_t)cmd_inputs[0].as_val->GetInt()));
    } else if (cmd_inputs[0].as_val->is_uint()) {
      frame.reg[result.reg] = Value::Type(Arr(cmd_inputs[1].as_val->GetType(),
                                              cmd_inputs[0].as_val->GetUint()));
    } 
  } break;
  case Op::Bytes: {
    frame.reg[result.reg] =
        Value::Uint(cmd_inputs[0].as_val->GetType()->bytes());
  } break;
  case Op::Alignment: {
    frame.reg[result.reg] =
        Value::Uint(cmd_inputs[0].as_val->GetType()->alignment());
  } break;
  case Op::Trunc: {
    frame.reg[result.reg] = Value::Char((char)cmd_inputs[0].as_val->GetUint());
    break;
  }
  case Op::ZExt: {
    frame.reg[result.reg] =
        Value::Uint((size_t)cmd_inputs[0].as_val->GetChar());
    break;
  }
  case Op::ArrayLength: {
    if (cmd_inputs[0].flag == ValType::Loc &&
        cmd_inputs[0].as_loc->is_stack_addr()) {
      frame.reg[result.reg] = cmd_inputs[0];

    } else if (cmd_inputs[0].flag == ValType::HeapAddr) {
      NOT_YET;
    } else {
      UNREACHABLE;
    }
  } break;
  case Op::ArrayData: {
    if (cmd_inputs[1].flag == ValType::Loc &&
        cmd_inputs[0].as_loc->is_stack_addr()) {
      frame.reg[result.reg] = IR::Value::StackAddr(
          cmd_inputs[1].as_loc->GetStackAddr() + sizeof(char *));

    } else if (cmd_inputs[1].flag == ValType::HeapAddr) {
      NOT_YET;
    } else {
      UNREACHABLE;
    }
  } break;
  case Op::PushField: {
    // NOTE: Hack. It's not a heap address, it's a vector pointer
    auto vec_ptr =
        (std::vector<std::tuple<const char *, Value, Value>> *)cmd_inputs[0]
            .as_heap_addr;
    vec_ptr->emplace_back(cmd_inputs[1].as_cstr, cmd_inputs[2], cmd_inputs[3]);
  } break;
  case Op::InitFieldVec: {
    auto vec_ptr = new std::vector<std::tuple<const char *, Value, Value>>;
    vec_ptr->reserve(cmd_inputs[0].as_val->GetUint());
    frame.reg[result.reg] = Value::HeapAddr(vec_ptr);
  } break;
  case Op::GetFromCache: {
    assert(cmd_inputs[0].as_val->GetType()->is_parametric_struct());
    auto param_struct_type = (ParamStruct *)cmd_inputs[0].as_val->GetType();

    // What if an argument isn't a type?
    for (auto a : frame.args) {
      assert(a.flag == ValType::Val && a.as_val->is_type());
    }

    auto iter = param_struct_type->cache.find(frame.args);
    if (iter == param_struct_type->cache.end()) {
      std::stringstream ss;
      ss << param_struct_type->bound_name << "(";
      if (!frame.args.empty()) { ss << frame.args[0]; }
      for (size_t i = 1; i < frame.args.size(); ++i) {
        ss << "," << frame.args[i];
      }
      ss << ")";
      auto struct_type = new Struct(ss.str());
      param_struct_type->cache[frame.args] = struct_type;
      frame.reg[result.reg] = IR::Value::Type(Err);
    } else {
      frame.reg[result.reg] = IR::Value::Type(iter->second);
    }
  } break;
  case Op::CreateStruct: {
    auto vec_ptr =
        (std::vector<std::tuple<const char *, Value, Value>> *)cmd_inputs[0]
            .as_heap_addr;

    auto param_struct = (ParamStruct *)cmd_inputs[1].as_heap_addr;
    // What if an argument isn't a type?
    for (auto a : frame.args) {
      assert(a.flag == ValType::Val && a.as_val->is_type());
    }

    auto struct_type = param_struct->cache[frame.args];
    assert(struct_type);
    param_struct->reverse_cache[struct_type] = frame.args;
    struct_type->creator                     = param_struct;

    Cursor loc;
    for (const auto &tuple_val : *vec_ptr) {
      auto id          = new AST::Identifier(loc, std::get<0>(tuple_val));
      auto decl        = new AST::Declaration;
      decl->identifier = id;
      id->decl         = decl;

      assert(std::get<1>(tuple_val).flag == ValType::Val &&
             std::get<1>(tuple_val).as_val->is_type());
      auto ty = std::get<1>(tuple_val).as_val->GetType();
      decl->type_expr = ty ? new AST::DummyTypeExpr(loc, ty) : nullptr;

      auto init_val = std::get<2>(tuple_val);
      if (init_val != IR::Value::None()) {
        // If it's not null,
        auto term      = new AST::Terminal;
        term->loc      = loc;
        decl->init_val = term;
        switch (init_val.flag) {
        case ValType::Val:
          if (init_val.as_val->is_bool()) {
            term->terminal_type = init_val.as_val->GetBool()
                                      ? Language::Terminal::True
                                      : Language::Terminal::False;
          term->type = Bool;
          } else if (init_val.as_val->is_char()) {
            term->terminal_type = Language::Terminal::Char;
            term->type          = Char;
          } else if (init_val.as_val->is_char()) {
            term->terminal_type = Language::Terminal::Int;
            term->type          = Int;
          } else if (init_val.as_val->is_real()) {
            term->terminal_type = Language::Terminal::Real;
            term->type          = Real;
          } else if (init_val.as_val->is_uint()) {
           term->terminal_type = Language::Terminal::Uint;
           term->type          = Uint;
          }
          break;

        default:
          frame.curr_block->dump();
          std::cerr << init_val << std::endl;
          NOT_YET;
        }
        term->value = init_val;
      }
      struct_type->decls.push_back(decl);
    }

    ScopeStack.push(struct_type->type_scope);
    for (auto d : struct_type->decls) { d->assign_scope(); }
    ScopeStack.pop();

    struct_type->CompleteDefinition();

    frame.reg[result.reg] = Value::Type(struct_type);
  } break;
  }
}

Block *Exit::ReturnVoid::JumpBlock(StackFrame &fr) { return nullptr; }
Block *Exit::Return::JumpBlock(StackFrame &fr) { return nullptr; }
Block *Exit::Unconditional::JumpBlock(StackFrame &fr) { return block; }
Block *Exit::Conditional::JumpBlock(StackFrame &fr) {
  Value val = ResolveValue(fr, cond);
  assert(val.flag == ValType::Val);
  return val.as_val->GetBool() ? true_block : false_block;
}

Block *Exit::Switch::JumpBlock(StackFrame &fr) {
  Value val = ResolveValue(fr, cond);
  switch (val.flag) {
  case ValType::Val:
    for (auto entry : table) {
      if (entry.first.as_val->GetChar() != val.as_val->GetChar()) { continue; }
      return entry.second;
    }
    return default_block;
  default: UNREACHABLE;
  }
}

StackFrame::StackFrame(Func *f, LocalStack *local_stack,
                       const std::vector<Value> &args)
    : reg(f->num_cmds), stack(local_stack), args(args), func(f), alignment(16),
      size(f->frame_size), offset(0), inst_ptr(0), curr_block(f->entry()),
      prev_block(nullptr) {
  stack->AddFrame(this);
  // TODO determine frame alignment and make it correct! Currently just assuming
  // 16-bytes for safety.
}

void LocalStack::AddFrame(StackFrame *fr) {
  size_t old_use_size = used;
  used       = MoveForwardToAlignment(used, fr->alignment);
  fr->offset = used;
  used += fr->size;

  if (used <= capacity) { return; }

  while (used > capacity) { capacity *= 2; }
  char *new_allocs = (char *)malloc(capacity);
  memcpy(new_allocs, allocs, old_use_size);
  free(allocs);
  allocs = new_allocs;
}

void LocalStack::RemoveFrame(StackFrame *fr) { used = fr->offset; }

Value IR::Func::Call(LocalStack *local_stack,
                     const std::vector<Value> &arg_vals) {
  StackFrame frame(this, local_stack, arg_vals);

eval_loop_start:
  if (frame.inst_ptr == frame.curr_block->cmds.size()) {
    if (debug::ct_eval) { RefreshDisplay(frame, frame.stack); }
    frame.prev_block = frame.curr_block;
    frame.curr_block = frame.curr_block->exit->JumpBlock(frame);

    if (!frame.curr_block) { // It's a return (perhaps void)
      return (frame.prev_block->exit->is_return())
                 ? ResolveValue(
                       frame, ((Exit::Return *)frame.prev_block->exit)->ret_val)
                 : Value::None();
    } else {
      frame.inst_ptr = 0;
    }

  } else {
    if (debug::ct_eval) { RefreshDisplay(frame, frame.stack); }

    frame.curr_block->cmds[frame.inst_ptr].Execute(frame);
    ++frame.inst_ptr;
  }
  goto eval_loop_start;
}
} // namespace IR
