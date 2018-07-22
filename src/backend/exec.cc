#include "backend/exec.h"

#include <iomanip>
#include <cmath>
#include <cstring>
#include <future>
#include <iostream>
#include <memory>

#include "architecture.h"
#include "ast/codeblock.h"
#include "ast/expression.h"
#include "ast/function_literal.h"
#include "base/util.h"
#include "context.h"
#include "error/log.h"
#include "ir/func.h"
#include "module.h"
#include "type/all.h"

using base::check::Is;

// TODO compile-time failure. dump the stack trace and abort for Null address
// kinds

namespace IR {
template <typename T>
T ExecContext::resolve(Register val) const {
  auto &reg_map = call_stack.top().fn_->reg_map_;
  auto iter     = reg_map.find(val);
  if (iter == reg_map.end()) {
    return std::get<T>(reg(val).value);
  } else {
    return call_stack.top().regs_.get<T>(iter->second);
  }
}

extern Val MakeBlockSeq(const base::vector<Val> &blocks);

base::vector<Val> Execute(Func *fn, const base::vector<Val> &arguments,
                          ExecContext *ctx) {
  if (fn->gened_fn_) { fn->gened_fn_->CompleteBody(fn->mod_); }
  ctx->call_stack.emplace(fn, arguments);

  for (i32 i = 0; i < static_cast<i32>(arguments.size()); ++i) {
    std::visit(
        [&](auto arg) {
          if constexpr (std::is_trivially_copyable_v<
                            std::decay_t<decltype(arg)>>) {
            ctx->call_stack.top().regs_.set(
                ctx->call_stack.top().fn_->reg_map_.at(Register{i}), arg);
          } else {
            UNREACHABLE();
          }
        },
        arguments[i].value);
  }

  // TODO log an error if you're asked to execute a function that had an error.

  while (true) {
    auto block_index = ctx->ExecuteBlock();
    if (block_index.is_default()) {
      base::vector<IR::Val> rets;
      auto *fn_type = ctx->call_stack.top().fn_->type_;
      for (size_t i = fn_type->input.size();
           i < fn_type->input.size() + fn_type->output.size(); ++i) {
        rets.push_back(std::move(ctx->reg(Register(i))));
      }
      ctx->call_stack.pop();
      return rets;
    } else {
      ctx->call_stack.top().MoveTo(block_index);
    }
  }
}

ExecContext::ExecContext() : stack_(50u) {}

BasicBlock &ExecContext::current_block() {
  return call_stack.top().fn_->block(call_stack.top().current_);
}

ExecContext::Frame::Frame(Func *fn, const base::vector<Val> &arguments)
    : fn_(fn),
      current_(fn_->entry()),
      prev_(fn_->entry()),
      old_regs_(fn->num_regs_, Val::None()),
      regs_(base::untyped_buffer::MakeFull(fn_->reg_size_)) {
  size_t num_inputs = fn->type_->input.size();
  ASSERT(num_inputs <= arguments.size());
  ASSERT(num_inputs <= old_regs_.size());
  for (size_t i = 0; i < arguments.size(); ++i) { old_regs_[i] = arguments[i]; }
}

BlockIndex ExecContext::ExecuteBlock() {
  Val result;
  ASSERT(current_block().cmds_.size() > 0u);
  auto cmd_iter = current_block().cmds_.begin();
  do {
    result = ExecuteCmd(*cmd_iter);
    if (cmd_iter->type != nullptr && cmd_iter->type != type::Void()) {
      // TODO the below assertion fails for structs (actual struct vs ptr to the
      // struct). Figure out how to fix it
      // ASSERT(result.type == cmd_iter->type);
      if (result == IR::Val::None()) {
        /*
        auto iter = call_stack.top().fn_->reg_map_.find(cmd_iter->result);
        if (cmd_iter->type == type::Bool) {
          this->reg(cmd_iter->result) =
              IR::Val::Bool(call_stack.top().regs_.get<bool>(iter->second));
        } else if (cmd_iter->type == type::Char) {
          this->reg(cmd_iter->result) =
              IR::Val::Char(call_stack.top().regs_.get<char>(iter->second));
        } else if (cmd_iter->type == type::Int) {
          this->reg(cmd_iter->result) =
              IR::Val::Int(call_stack.top().regs_.get<i32>(iter->second));
        } else if (cmd_iter->type == type::Real) {
          this->reg(cmd_iter->result) =
              IR::Val::Real(call_stack.top().regs_.get<double>(iter->second));
        } else if (cmd_iter->type == type::Type_) {
          this->reg(cmd_iter->result) = IR::Val::Type(
              call_stack.top().regs_.get<type::Type const *>(iter->second));
        } else if (cmd_iter->type ->is<type::CharBuffer>()) {
          // TODO Unnecessary copy and global string table lookup.
          this->reg(cmd_iter->result) = IR::Val::CharBuf(std::string(
              call_stack.top().regs_.get<std::string_view>(iter->second)));
        } else if (cmd_iter->type->is<type::Pointer>()) {
          this->reg(cmd_iter->result) =
              IR::Val::Addr(call_stack.top().regs_.get<Addr>(iter->second),
                            cmd_iter->type->as<type::Pointer>().pointee);
        } else {
          cmd_iter->dump(10);
          NOT_YET(cmd_iter->type->to_string());
        }
        */
      } else {
        this->reg(cmd_iter->result) = result;
      }
    }
    ++cmd_iter;
  } while (!std::holds_alternative<BlockIndex>(result.value));

  return std::get<BlockIndex>(result.value);
}

IR::Addr Stack::Push(const type::Pointer *ptr) {
  Addr addr;
  addr.as_stack = buffer_.size();
  addr.kind     = Addr::Kind::Stack;
  buffer_.append_bytes(
      Architecture::InterprettingMachine().bytes(ptr->pointee));
  return addr;
}

template <typename T>
static T LoadValue(Addr addr, const Stack &stack) {
  switch (addr.kind) {
    case Addr::Kind::Null: UNREACHABLE();
    case Addr::Kind::Heap: return *static_cast<T *>(addr.as_heap); break;
    case Addr::Kind::Stack: return stack.Load<T>(addr.as_stack); break;
  }
  UNREACHABLE();
}

template <typename T>
static void StoreValue(T val, Addr addr, Stack *stack) {
  switch (addr.kind) {
    case Addr::Kind::Null:
      // TODO compile-time failure. dump the stack trace and abort.
      UNREACHABLE();
    case Addr::Kind::Stack: stack->Store(val, addr.as_stack); return;
    case Addr::Kind::Heap: *static_cast<T *>(addr.as_heap) = val;
  }
}

Val ExecContext::ExecuteCmd(const Cmd &cmd) {
  auto save = [&](auto val) {
    call_stack.top().regs_.set(call_stack.top().fn_->reg_map_.at(cmd.result),
                               val);
  };

  switch (cmd.op_code_) {
    case Op::Trunc:
      save(static_cast<char>(resolve<i32>(cmd.trunc_.reg_)));
      break;
    case Op::Extend:
      save(static_cast<i32>(resolve<char>(cmd.extend_.reg_)));
      break;
    case Op::Bytes:
      save(
          Architecture::InterprettingMachine().bytes(resolve(cmd.bytes_.arg_)));
      break;
    case Op::Align:
      save(Architecture::InterprettingMachine().alignment(
          resolve(cmd.align_.arg_)));
      break;
    case Op::Not: save(!resolve<bool>(cmd.not_.reg_)); break;
    case Op::NegInt: save(-resolve<i32>(cmd.neg_int_.reg_)); break;
    case Op::NegReal: save(-resolve<double>(cmd.neg_real_.reg_)); break;
    case Op::ArrayLength: save(resolve(cmd.array_data_.arg_)); break;
    case Op::ArrayData: {
      auto addr = resolve(cmd.array_data_.arg_);
      switch (addr.kind) {
        case Addr::Kind::Null: UNREACHABLE();
        case Addr::Kind::Stack:
          save(addr.as_stack +
               Architecture::InterprettingMachine().bytes(type::Int));
          break;
        case Addr::Kind::Heap:
          save(static_cast<void *>(
              static_cast<u8 *>(addr.as_heap) +
              Architecture::InterprettingMachine().bytes(type::Int)));
          break;
      }
    } break;
    case Op::LoadBool:
      save(LoadValue<bool>(resolve(cmd.load_bool_.arg_), stack_));
      break;
    case Op::LoadChar:
      save(LoadValue<char>(resolve(cmd.load_char_.arg_), stack_));
      break;
    case Op::LoadInt:
      save(LoadValue<i32>(resolve(cmd.load_int_.arg_), stack_));
      break;
    case Op::LoadReal:
      save(LoadValue<double>(resolve(cmd.load_real_.arg_), stack_));
      break;
    case Op::LoadType:
      save(LoadValue<type::Type const *>(resolve(cmd.load_type_.arg_), stack_));
      break;
    case Op::LoadEnum:
      save(LoadValue<size_t>(resolve(cmd.load_enum_.arg_), stack_));
      break;
    case Op::LoadFlags:
      save(LoadValue<size_t>(resolve(cmd.load_flags_.arg_), stack_));
      break;
    case Op::LoadAddr:
      save(LoadValue<Addr>(resolve(cmd.load_addr_.arg_), stack_));
      break;
      /*
      switch (addr.kind) {
        case Addr::Kind::Null: UNREACHABLE();
        case Addr::Kind::Heap: {
          // LOAD_FROM_HEAP(type::Code, IR::Val::CodeBlock, AST::CodeBlock);
          if (cmd.type->is<type::Pointer>()) {
            return IR::Val::Addr(*static_cast<Addr *>(addr.as_heap),
                                 cmd.type->as<type::Pointer>().pointee);
          } else if (cmd.type->is<type::CharBuffer>()) {
            // TODO Add a string_view overload for Val::CharBuf.
            return IR::Val::CharBuf(
                std::string(*static_cast<std::string_view *>(addr.as_heap)));
          } else {
            NOT_YET("Don't know how to load type: ", cmd.type);
          }
          NOT_YET("Don't know how to load type: ", cmd.type);
        } break;
        case Addr::Kind::Stack: {
          // TODO LOAD_FROM_STACK(type::Code, CodeBlock, AST::CodeBlock);
          if (cmd.type->is<type::Pointer>()) {
            switch (addr.kind) {
              case Addr::Kind::Stack:
                return IR::Val::Addr(stack_.Load<Addr>(addr.as_stack),
                                     cmd.type->as<type::Pointer>().pointee);
              case Addr::Kind::Heap:
                return IR::Val::Addr(*static_cast<Addr *>(addr.as_heap),
                                     cmd.type->as<type::Pointer>().pointee);
              case Addr::Kind::Null: NOT_YET();
            }
          } else if (cmd.type->is<type::CharBuffer>()) {
            // TODO Add a string_view overload for Val::CharBuf.
            return IR::Val::CharBuf(
                std::string(stack_.Load<std::string_view>(addr.as_stack)));
          } else if (cmd.type->is<type::Function>()) {
            return IR::Val::Func(stack_.Load<IR::Func *>(addr.as_stack));
          } else {
            call_stack.top().fn_->dump();
            NOT_YET("Don't know how to load type: ", cmd.type);
          }
        } break;
      }*/
    case Op::AddInt:
      save(resolve(cmd.add_int_.args_[0]) + resolve(cmd.add_int_.args_[1]));
      break;
    case Op::AddReal:
      save(resolve(cmd.add_real_.args_[0]) + resolve(cmd.add_real_.args_[1]));
      break;
    case Op::AddCharBuf:
      // TODO remove to/from Val. it's a waste.
      save(std::get<std::string_view>(
          IR::Val::CharBuf(std::string(resolve(cmd.add_char_buf_.args_[0])) +
                           std::string(resolve(cmd.add_char_buf_.args_[1])))
              .value));
      break;
    case Op::SubInt:
      save(resolve(cmd.sub_int_.args_[0]) - resolve(cmd.sub_int_.args_[1]));
      break;
    case Op::SubReal:
      save(resolve(cmd.sub_real_.args_[0]) - resolve(cmd.sub_real_.args_[1]));
      break;
    case Op::MulInt:
      save(resolve(cmd.mul_int_.args_[0]) * resolve(cmd.mul_int_.args_[1]));
      break;
    case Op::MulReal:
      save(resolve(cmd.mul_real_.args_[0]) * resolve(cmd.mul_real_.args_[1]));
      break;
    case Op::DivInt:
      save(resolve(cmd.div_int_.args_[0]) / resolve(cmd.div_int_.args_[1]));
      break;
    case Op::DivReal:
      save(resolve(cmd.div_real_.args_[0]) / resolve(cmd.div_real_.args_[1]));
      break;
    case Op::ModInt:
      save(resolve(cmd.mod_int_.args_[0]) % resolve(cmd.mod_int_.args_[1]));
      break;
    case Op::ModReal:
      save(std::fmod(resolve(cmd.mod_real_.args_[0]),
                     resolve(cmd.mod_real_.args_[1])));
      break;
    case Op::LtInt:
      save(resolve(cmd.lt_int_.args_[0]) < resolve(cmd.lt_int_.args_[1]));
      break;
    case Op::LtReal:
      save(resolve(cmd.lt_real_.args_[0]) < resolve(cmd.lt_real_.args_[1]));
      break;
    case Op::LtFlags: {
      auto lhs = resolve(cmd.lt_flags_.args_[0]);
      auto rhs = resolve(cmd.lt_flags_.args_[1]);
      save(lhs.value != rhs.value && ((lhs.value | rhs.value) == rhs.value));
    } break;
    case Op::LeInt:
      save(resolve(cmd.le_int_.args_[0]) <= resolve(cmd.le_int_.args_[1]));
      break;
    case Op::LeReal:
      save(resolve(cmd.le_real_.args_[0]) <= resolve(cmd.le_real_.args_[1]));
      break;
    case Op::LeFlags: {
      auto lhs = resolve(cmd.le_flags_.args_[0]);
      auto rhs = resolve(cmd.le_flags_.args_[1]);
      save((lhs.value | rhs.value) == rhs.value);
    } break;
    case Op::GtInt:
      save(resolve(cmd.gt_int_.args_[0]) > resolve(cmd.gt_int_.args_[1]));
      break;
    case Op::GtReal:
      save(resolve(cmd.gt_real_.args_[0]) > resolve(cmd.gt_real_.args_[1]));
      break;
    case Op::GtFlags: {
      auto lhs = resolve(cmd.gt_flags_.args_[0]);
      auto rhs = resolve(cmd.gt_flags_.args_[1]);
      save(lhs.value != rhs.value && ((lhs.value | rhs.value) == lhs.value));
    } break;
    case Op::GeInt:
      save(resolve(cmd.ge_int_.args_[0]) >= resolve(cmd.ge_int_.args_[1]));
      break;
    case Op::GeReal:
      save(resolve(cmd.ge_real_.args_[0]) >= resolve(cmd.ge_real_.args_[1]));
      break;
    case Op::GeFlags: {
      auto lhs = resolve(cmd.ge_flags_.args_[0]);
      auto rhs = resolve(cmd.ge_flags_.args_[1]);
      save((lhs.value | rhs.value) == lhs.value);
    } break;
    case Op::EqBool:
      save(resolve(cmd.eq_bool_.args_[0]) == resolve(cmd.eq_bool_.args_[1]));
      break;
    case Op::EqChar:
      save(resolve(cmd.eq_char_.args_[0]) == resolve(cmd.eq_char_.args_[1]));
      break;
    case Op::EqInt:
      save(resolve(cmd.eq_int_.args_[0]) == resolve(cmd.eq_int_.args_[1]));
      break;
    case Op::EqReal:
      save(resolve(cmd.eq_real_.args_[0]) == resolve(cmd.eq_real_.args_[1]));
      break;
    case Op::EqFlags:
      save(resolve(cmd.eq_flags_.args_[0]) == resolve(cmd.eq_flags_.args_[1]));
      break;
    case Op::EqType:
      save(resolve(cmd.eq_type_.args_[0]) == resolve(cmd.eq_type_.args_[1]));
      break;
    case Op::EqAddr:
      save(resolve(cmd.eq_addr_.args_[0]) == resolve(cmd.eq_addr_.args_[1]));
      break;
    case Op::NeBool:
      save(resolve(cmd.ne_bool_.args_[0]) == resolve(cmd.ne_bool_.args_[1]));
      break;
    case Op::NeChar:
      save(resolve(cmd.ne_char_.args_[0]) == resolve(cmd.ne_char_.args_[1]));
      break;
    case Op::NeInt:
      save(resolve(cmd.ne_int_.args_[0]) == resolve(cmd.ne_int_.args_[1]));
      break;
    case Op::NeReal:
      save(resolve(cmd.ne_real_.args_[0]) == resolve(cmd.ne_real_.args_[1]));
      break;
    case Op::NeFlags:
      save(resolve(cmd.ne_flags_.args_[0]) == resolve(cmd.ne_flags_.args_[1]));
      break;
    case Op::NeType:
      save(resolve(cmd.ne_type_.args_[0]) == resolve(cmd.ne_type_.args_[1]));
      break;
    case Op::NeAddr:
      save(resolve(cmd.ne_addr_.args_[0]) == resolve(cmd.ne_addr_.args_[1]));
      break;
    case Op::XorBool:
      save(resolve(cmd.xor_bool_.args_[0]) ^ resolve(cmd.xor_bool_.args_[1]));
      break;
    case Op::XorFlags: NOT_YET();
    case Op::OrBool:
      save(resolve(cmd.or_bool_.args_[0]) | resolve(cmd.or_bool_.args_[1]));
      break;
    case Op::OrFlags: NOT_YET();
    case Op::AndBool:
      save(resolve(cmd.and_bool_.args_[0]) & resolve(cmd.and_bool_.args_[1]));
      break;
    case Op::AndFlags: NOT_YET();
    case Op::CreateStruct: save(new type::Struct); break;
    case Op::InsertField: {
      std::vector<Val> resolved_args;
      resolved_args.reserve(ASSERT_NOT_NULL(cmd.insert_field_.args_)->size());
      for (auto const &arg : *cmd.insert_field_.args_) {
        if (auto *r = std::get_if<Register>(&arg.value)) {
          resolved_args.push_back(reg(*r));
        } else {
          resolved_args.push_back(arg);
        }
      }

      auto *struct_to_mod = std::get<type::Struct *>(resolved_args[0].value);
      struct_to_mod->fields_.push_back(type::Struct::Field{
          std::string(std::get<std::string_view>(resolved_args[1].value)),
          std::get<const type::Type *>(resolved_args[2].value),
          resolved_args[3]});

      auto[iter, success] = struct_to_mod->field_indices_.emplace(
          std::string(std::get<std::string_view>(resolved_args[1].value)),
          struct_to_mod->fields_.size() - 1);
      ASSERT(success);
    } break;
    case Op::FinalizeStruct:
      save(resolve<type::Struct *>(cmd.finalize_struct_.reg_)->finalize());
      break;
    case Op::Malloc: save(malloc(resolve(cmd.malloc_.arg_))); break;
    case Op::Free:
      free(std::get<Addr>(reg(cmd.free_.reg_).value).as_heap);
      break;
    case Op::Alloca: save(stack_.Push(&cmd.type->as<type::Pointer>())); break;
    case Op::Ptr:
      save(type::Ptr(resolve<type::Type const *>(cmd.ptr_.reg_)));
      break;
    case Op::Arrow:
      save(type::Func({resolve(cmd.arrow_.args_[0])},
                      {resolve(cmd.arrow_.args_[1])}));
      break;
    case Op::Array: {
      auto len = resolve(cmd.array_.len_);
      auto t   = resolve(cmd.array_.type_);
      save(len == -1 ? type::Arr(t) : type::Arr(t, len));
    } break;
    case Op::VariantType: save(resolve<Addr>(cmd.variant_type_.reg_)); break;
    case Op::VariantValue: {
      auto bytes = Architecture::InterprettingMachine().bytes(Ptr(type::Type_));
      auto bytes_fwd =
          Architecture::InterprettingMachine().MoveForwardToAlignment(
              Ptr(type::Type_), bytes);
      auto addr = resolve<Addr>(cmd.variant_value_.reg_);
      switch (addr.kind) {
        case Addr::Kind::Stack: save(addr.as_stack + bytes_fwd); break;
        case Addr::Kind::Heap:
          save(static_cast<char *>(addr.as_heap) + bytes_fwd);
          break;
        case Addr::Kind::Null: NOT_YET();
      }
    } break;
    case Op::PtrIncr: {
      auto addr = resolve<Addr>(cmd.ptr_incr_.ptr_);
      auto incr = resolve(cmd.ptr_incr_.incr_);
      // Sadly must convert to value and back even though it's guaranteed to be
      // constant folded
      auto bytes_fwd = std::get<i32>(
          Architecture::InterprettingMachine()
              .ComputeArrayLength(IR::Val::Int(incr),
                                  cmd.type->as<type::Pointer>().pointee)
              .value);
      switch (addr.kind) {
        case Addr::Kind::Stack: save(addr.as_stack + bytes_fwd); break;
        case Addr::Kind::Heap:
          save(static_cast<char *>(addr.as_heap) + bytes_fwd);
          break;
        case Addr::Kind::Null: NOT_YET();
      }
    } break;
    case Op::Field: {
      auto addr_val = reg(cmd.field_.ptr_);
      auto addr     = std::get<Addr>(addr_val.value);

      auto *struct_type =
          &addr_val.type->as<type::Pointer>().pointee->as<type::Struct>();
      // This can probably be precomputed.
      size_t offset = 0;
      for (size_t i = 0; i < cmd.field_.num_; ++i) {
        auto field_type = struct_type->fields_.at(i).type;
        offset += Architecture::InterprettingMachine().bytes(field_type);
        offset = Architecture::InterprettingMachine().MoveForwardToAlignment(
            struct_type->fields_.at(i + 1).type, offset);
      }

      if (addr.kind == Addr::Kind::Stack) {
        save(addr.as_stack + offset);
      } else {
        save(static_cast<char *>(addr.as_heap) + offset);
      }
    } break;
    case Op::PrintBool:
      std::cerr << (resolve(cmd.print_bool_.arg_) ? "true" : "false");
      break;
    case Op::PrintChar: std::cerr << resolve(cmd.print_char_.arg_); break;
    case Op::PrintInt: std::cerr << resolve(cmd.print_int_.arg_); break;
    case Op::PrintReal: std::cerr << resolve(cmd.print_real_.arg_); break;
    case Op::PrintType:
      std::cerr << resolve(cmd.print_type_.arg_)->to_string();
      break;
    case Op::PrintEnum:
      NOT_YET();
      /*
      std::cerr << resolved[0].type->as<type::Enum>().members_[e.value];
      */
    case Op::PrintFlags:
      NOT_YET();
      /*
      size_t val = f.value;
      base::vector<std::string> vals;
      const auto &members = resolved[0].type->as<type::Flags>().members_;
      size_t i            = 0;
      size_t pow          = 1;
      while (pow <= val) {
        if (val & pow) { vals.push_back(members[i]); }
        ++i;
        pow <<= 1;
      }
      if (vals.empty()) {
        std::cerr << "(empty)";
      } else {
        auto iter = vals.begin();
        std::cerr << *iter++;
        while (iter != vals.end()) { std::cerr << " | " << *iter++; }
      }
      */
      break;
    case Op::PrintAddr:
      std::cerr << resolve(cmd.print_addr_.arg_).to_string();
      break;
    case Op::PrintCharBuffer:
      std::cerr << resolve(cmd.print_char_buffer_.arg_);
      break;
    case Op::Call: {
      std::vector<Val> resolved_args;
      resolved_args.reserve(ASSERT_NOT_NULL(cmd.call_.args_)->size());
      for (auto const &arg : *cmd.call_.args_) {
        if (auto *r = std::get_if<Register>(&arg.value)) {
          if (arg.type == type::Bool) {
            resolved_args.push_back(IR::Val::Bool(resolve<bool>(*r)));
          } else if (arg.type == type::Char) {
            resolved_args.push_back(IR::Val::Char(resolve<char>(*r)));
          } else if (arg.type == type::Int) {
            resolved_args.push_back(IR::Val::Int(resolve<i32>(*r)));
          } else if (arg.type == type::Real) {
            resolved_args.push_back(IR::Val::Real(resolve<double>(*r)));
          } else if (arg.type == type::Type_) {
            resolved_args.push_back(
                IR::Val::Type(resolve<type::Type const *>(*r)));
          } else {
            NOT_YET(arg.type->to_string());
          }
        } else {
          resolved_args.push_back(arg);
        }
      }

      // TODO you need to be able to determine how many args there are
      switch (cmd.call_.which_active_) {
        case 0x00: {
          // TODO what if the register is a foerign fn?
          auto results = Execute(std::get<Func *>(reg(cmd.call_.reg_).value),
                                 resolved_args, this);

          if (cmd.type == type::Bool) {
            save(std::get<bool>(results[0].value));
          } else if (cmd.type == type::Char) {
            save(std::get<char>(results[0].value));
          } else if (cmd.type == type::Int) {
            save(std::get<i32>(results[0].value));
          } else if (cmd.type == type::Real) {
            save(std::get<double>(results[0].value));
          } else if (cmd.type == type::Type_) {
            save(std::get<type::Type const *>(results[0].value));
          } else if (cmd.type->is<type::Pointer>()) {
            save(std::get<IR::Addr>(results[0].value));
          } else if (cmd.type == type::Block || cmd.type == type::OptBlock) {
            save(std::get<BlockSequence>(results[0].value));
          } else {
            NOT_YET(cmd.type->to_string());
          }

          return results.empty() ? IR::Val::None() : results[0];
        } break;
        case 0x01: {
          auto results = Execute(cmd.call_.fn_, resolved_args, this);
          if (cmd.type == type::Bool) {
            save(std::get<bool>(results[0].value));
          } else if (cmd.type == type::Char) {
            save(std::get<char>(results[0].value));
          } else if (cmd.type == type::Int) {
            save(std::get<i32>(results[0].value));
          } else if (cmd.type == type::Real) {
            save(std::get<double>(results[0].value));
          } else if (cmd.type == type::Type_) {
            save(std::get<type::Type const *>(results[0].value));
          } else if (cmd.type->is<type::Pointer>()) {
            save(std::get<IR::Addr>(results[0].value));
          } else if (cmd.type == type::Block || cmd.type == type::OptBlock) {
            save(std::get<BlockSequence>(results[0].value));
          } else {
            NOT_YET(cmd.type->to_string());
          }
          return results.empty() ? IR::Val::None() : results[0];
        } break;
        case 0x02:
          if (cmd.call_.foreign_fn_.name_ == "malloc") {
            return IR::Val::HeapAddr(
                malloc(std::get<i32>(resolved_args[0].value)),
                cmd.type->as<type::Pointer>().pointee);
          } else {
            NOT_YET();
          }
      }
    } break;
    case Op::Tup: {
      base::vector<const type::Type *> types;
      types.reserve(cmd.tup_.args_->size());
      for (const auto &val : *cmd.tup_.args_) {
        types.push_back(std::get<const type::Type *>(val.value));
      }
      save(type::Tup(std::move(types)));
    } break;
    case Op::Variant: {
      base::vector<const type::Type *> types;
      types.reserve(cmd.variant_.args_->size());
      for (const auto &val : *cmd.variant_.args_) {
        types.push_back(std::get<const type::Type *>(val.value));
      }
      save(type::Var(std::move(types)));
    } break;
    case Op::CastIntToReal:
      save(static_cast<double>(resolve<i32>(cmd.cast_int_to_real_.reg_)));
      break;
    case Op::CastPtr: save(resolve<IR::Addr>(cmd.cast_ptr_.reg_)); break;
    case Op::AddCodeBlock: NOT_YET();
    case Op::BlockSeq: {
      std::vector<Val> resolved_args;
      resolved_args.reserve(ASSERT_NOT_NULL(cmd.block_seq_.args_)->size());
      for (auto const &arg : *cmd.block_seq_.args_) {
        if (auto *r = std::get_if<Register>(&arg.value)) {
          resolved_args.push_back(reg(*r));
        } else {
          resolved_args.push_back(arg);
        }
      }
      save(std::get<BlockSequence>(MakeBlockSeq(resolved_args).value));
    } break;
    case Op::BlockSeqContains: {
      auto *seq =
          std::get<BlockSequence>(reg(cmd.block_seq_contains_.reg_).value).seq_;
      save(std::any_of(seq->begin(), seq->end(), [&](AST::BlockLiteral *lit) {
        return lit == cmd.block_seq_contains_.lit_;
      }));
    } break;
    case Op::SetReturn: {
      Register out_reg = std::get<Register>(cmd.args[1].value);
      const auto &arg = cmd.args[0];

      if (auto *r = std::get_if<Register>(&arg.value)) {
        auto save_to_reg = [&](auto val) {
          call_stack.top().regs_.set(call_stack.top().fn_->reg_map_.at(out_reg),
                                     val);
        };

        auto *t = cmd.args[1].type->as<type::Pointer>().pointee;
        if (t == type::Bool) {
          save_to_reg(resolve<bool>(*r));
          reg(out_reg) = IR::Val::Bool(resolve<bool>(*r));
        } else if (t == type::Char) {
          save_to_reg(resolve<char>(*r));
          reg(out_reg) = IR::Val::Char(resolve<char>(*r));
        } else if (t == type::Int) {
          save_to_reg(resolve<i32>(*r));
          reg(out_reg) = IR::Val::Int(resolve<i32>(*r));
        } else if (t == type::Real) {
          save_to_reg(resolve<double>(*r));
          reg(out_reg) = IR::Val::Real(resolve<double>(*r));
        } else if (t == type::Type_) {
          save_to_reg(resolve<type::Type const *>(*r));
          reg(out_reg) = IR::Val::Type(resolve<type::Type const *>(*r));
        } else if (t->is<type::Pointer>()) {
          save_to_reg(resolve<IR::Addr>(*r));
          // TODO push back resolved
        } else if (t == type::Block || t == type::OptBlock) {
          save_to_reg(resolve<BlockSequence>(*r));
          reg(out_reg) = IR::Val::BlockSeq(resolve<BlockSequence>(*r));
        } else if (t->is<type::CharBuffer>()) {
          save_to_reg(resolve<std::string_view>(*r));
          // TODO push back resolved
        } else if (t->is<type::Function>()) {
          // TODO what kind of function?
          save_to_reg(resolve<IR::Func *>(*r));
          reg(out_reg) = IR::Val::Func(resolve<IR::Func *>(*r));
        } else if (t->is<type::Scope>()) {
          // TODO what kind of function?
          save_to_reg(resolve<AST::ScopeLiteral *>(*r));
          reg(out_reg) = IR::Val::Scope(resolve<AST::ScopeLiteral *>(*r));

        } else {
          NOT_YET(t->to_string());
        }
      } else {
        reg(out_reg) = arg;
      }

      return IR::Val::None();
    } break;
    case Op::StoreBool:
      StoreValue(resolve(cmd.store_bool_.val_),
                 resolve<IR::Addr>(cmd.store_bool_.addr_), &stack_);
      break;
    case Op::StoreChar:
      StoreValue(resolve(cmd.store_char_.val_),
                 resolve<IR::Addr>(cmd.store_char_.addr_), &stack_);
      break;
    case Op::StoreInt:
      StoreValue(resolve(cmd.store_int_.val_),
                 resolve<IR::Addr>(cmd.store_int_.addr_), &stack_);
      break;
    case Op::StoreReal:
      StoreValue(resolve(cmd.store_real_.val_),
                 resolve<IR::Addr>(cmd.store_real_.addr_), &stack_);
      break;
    case Op::StoreType:
      StoreValue(resolve(cmd.store_type_.val_),
                 resolve<IR::Addr>(cmd.store_type_.addr_), &stack_);
      break;
    case Op::StoreEnum:
      StoreValue(resolve(cmd.store_enum_.val_),
                 resolve<IR::Addr>(cmd.store_enum_.addr_), &stack_);
      break;
    case Op::StoreFlags:
      StoreValue(resolve(cmd.store_flags_.val_),
                 resolve<IR::Addr>(cmd.store_flags_.addr_), &stack_);
      break;
    case Op::StoreAddr:
      StoreValue(resolve(cmd.store_addr_.val_),
                 resolve<IR::Addr>(cmd.store_addr_.addr_), &stack_);
      break;
    case Op::Phi: {
      std::vector<Val> resolved_args;
      resolved_args.reserve(ASSERT_NOT_NULL(cmd.phi_.args_)->size());
      for (auto const &arg : *cmd.phi_.args_) {
        if (auto *r = std::get_if<Register>(&arg.value)) {
          if (cmd.type == type::Bool) {
            resolved_args.push_back(IR::Val::Bool(resolve<bool>(*r)));
          } else if (cmd.type == type::Char) {
            resolved_args.push_back(IR::Val::Char(resolve<char>(*r)));
          } else if (cmd.type == type::Int) {
            resolved_args.push_back(IR::Val::Int(resolve<i32>(*r)));
          } else if (cmd.type == type::Real) {
            resolved_args.push_back(IR::Val::Real(resolve<double>(*r)));
          } else if (cmd.type->is<type::Pointer>()) {
            resolved_args.push_back(IR::Val::Addr(
                resolve<IR::Addr>(*r), cmd.type->as<type::Pointer>().pointee));
          } else {
            NOT_YET(cmd.type->to_string());
          }
        } else {
          resolved_args.push_back(arg);
        }
      }

      for (size_t i = 0; i < resolved_args.size(); i += 2) {
        if (call_stack.top().prev_ ==
            std::get<IR::BlockIndex>(resolved_args[i].value)) {
          if (cmd.type == type::Bool) {
            save(std::get<bool>(resolved_args[i + 1].value));
          } else if (cmd.type == type::Char) {
            save(std::get<char>(resolved_args[i + 1].value));
          } else if (cmd.type == type::Int) {
            save(std::get<i32>(resolved_args[i + 1].value));
          } else if (cmd.type == type::Real) {
            save(std::get<double>(resolved_args[i + 1].value));
          } else if (cmd.type == type::Type_) {
            save(std::get<type::Type const *>(resolved_args[i + 1].value));
          } else if (cmd.type->is<type::Pointer>()) {
            save(std::get<IR::Addr>(resolved_args[i + 1].value));
          } else if (cmd.type == type::Block || cmd.type == type::OptBlock) {
            save(std::get<BlockSequence>(resolved_args[i + 1].value));
          } else {
            NOT_YET(cmd.type->to_string());
          }
          return resolved_args[i + 1];
        }
      }
      call_stack.top().fn_->dump();
      UNREACHABLE(
          "Previous block was ", Val::BasicBlock(call_stack.top().prev_),
          "\nCurrent block is ", Val::BasicBlock(call_stack.top().current_));
    } break;
    case Op::Contextualize: NOT_YET();
    case Op::CondJump:
      return Val::BasicBlock(
          cmd.cond_jump_.blocks_[resolve<bool>(cmd.cond_jump_.cond_)]);
    case Op::UncondJump: return Val::BasicBlock(cmd.uncond_jump_.block_);
    case Op::ReturnJump: return Val::BasicBlock(BlockIndex{-1});
  }
  return IR::Val::None();
}
}  // namespace IR

void ReplEval(AST::Expression *expr) {
  // TODO is nullptr for module okay here?
  auto fn = std::make_unique<IR::Func>(
      nullptr, type::Func({}, {}),
      base::vector<std::pair<std::string, AST::Expression *>>{});
  CURRENT_FUNC(fn.get()) {
    IR::BasicBlock::Current = fn->entry();
    // TODO use the right module
    Context ctx(nullptr);
    // TODO support multiple values computed simultaneously?
    auto expr_val = expr->EmitIR(&ctx)[0];
    if (ctx.num_errors() != 0) {
      ctx.DumpErrors();
      return;
    }

    if (expr->type != type::Void()) { expr->type->EmitRepr(expr_val, &ctx); }
    IR::ReturnJump();
  }

  IR::ExecContext ctx;
  Execute(fn.get(), {}, &ctx);
}
