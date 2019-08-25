#include "ir/cmd.h"

#include <cmath>
#include <iostream>
#include <vector>

#include "ast/ast.h"
#include "base/bag.h"
#include "core/arch.h"
#include "ir/cmd/jumps.h"
#include "ir/cmd/register.h"
#include "ir/compiled_fn.h"
#include "ir/reg.h"
#include "type/generic_struct.h"
#include "type/jump.h"
#include "type/typed_value.h"

namespace ir {
thread_local BlockIndex BasicBlock::Current;

Cmd &MakeCmd(type::Type const *t, Op op) {
  auto &blk = ASSERT_NOT_NULL(CompiledFn::Current)->block(BasicBlock::Current);
  auto &cmd = *blk.cmds_.emplace_back(std::make_unique<Cmd>(t, op));
  blk.cmd_buffer_.append_index<LegacyCmd>();
  DEBUG_LOG("LegacyCmd")(&cmd);
  blk.cmd_buffer_.append(&cmd);
  return cmd;
}

Reg CreateScopeDef(::Module const *mod, ScopeDef*scope_def) {
  auto &cmd = MakeCmd(type::Scope, Op::CreateScopeDef);
  // TODO you don't need the module.
  cmd.create_scope_def_ = {const_cast<::Module *>(mod), scope_def};
  return cmd.result;
}

Reg CreateBlockDef(ast::BlockLiteral const *parent) {
  auto &cmd      = MakeCmd(type::Block, Op::CreateBlockDef);
  cmd.block_lit_ = parent;
  return cmd.result;
}

void FinishScopeDef() { MakeCmd(type::Block, Op::FinishScopeDef); }

void FinishBlockDef(std::string_view name) {
  auto &cmd          = MakeCmd(nullptr, Op::FinishBlockDef);
  cmd.byte_view_arg_ = name;
}

void AddBlockDefBefore(RegOr<AnyFunc> f) {
  auto &cmd   = MakeCmd(nullptr, Op::AddBlockDefBefore);
  cmd.any_fn_ = f;
}

void AddBlockDefAfter(RegOr<AnyFunc> f) {
  auto &cmd   = MakeCmd(nullptr, Op::AddBlockDefAfter);
  cmd.any_fn_ = f;
}

void AddScopeDefInit(Reg reg, RegOr<AnyFunc> f) {
  auto &cmd               = MakeCmd(nullptr, Op::AddScopeDefInit);
  cmd.add_scope_def_init_ = {reg, f};
}

void AddScopeDefDone(Reg reg, RegOr<AnyFunc> f) {
  auto &cmd               = MakeCmd(nullptr, Op::AddScopeDefDone);
  cmd.add_scope_def_done_ = {reg, f};
}

BasicBlock &GetBlock() {
  return ASSERT_NOT_NULL(CompiledFn::Current)->block(BasicBlock::Current);
}

Reg Reserve(core::Bytes b, core::Alignment a) {
  return CompiledFn::Current->Reserve(b, a);
}
Reg Reserve(type::Type const *t) { return CompiledFn::Current->Reserve(t); }

Cmd::Cmd(type::Type const *t, Op op) : op_code_(op) {
  ASSERT(CompiledFn::Current != nullptr);
  CmdIndex cmd_index{
      BasicBlock::Current,
      static_cast<int32_t>(
          CompiledFn::Current->block(BasicBlock::Current).cmds_.size())};
  if (t == nullptr) {
    result = Reg();
    CompiledFn::Current->reg_to_cmd_.emplace(result, cmd_index);
    return;
  }

  result = Reserve(t);

  // TODO this isn't done for cmd-buffer commands and needs to be eventually, at
  // least for phi nodes. properties want it to.
  // TODO for implicitly declared out-params of a Call, map them to the call.
  CompiledFn::Current->reg_to_cmd_.emplace(result, cmd_index);
}

TypedRegister<Addr> Alloca(type::Type const *t) {
  return CompiledFn::Current->Alloca(t);
}

TypedRegister<Addr> TmpAlloca(type::Type const *t, Context *ctx) {
  auto reg = Alloca(t);
  ctx->temporaries_to_destroy_->emplace_back(reg, t);
  return reg;
}

std::pair<Results, bool> CallInline(
    CompiledFn *f, Arguments const &arguments,
    absl::flat_hash_map<ir::BlockDef const *, ir::BlockIndex> const
        &block_map) {

  bool is_jump = false; // TODO remove this
  std::vector<Results> return_vals;
  return_vals.resize(f->type_->output.size());

  // Note: It is important that the inliner is created before making registers
  // for each of the arguments, because creating the inliner looks state on the
  // current function (counting which register it should start on), and this
  // should exclude the registers we create to hold the arguments.
  auto inliner = CompiledFn::Current->inliner();

  std::vector<Reg> arg_regs;
  arg_regs.reserve(f->type_->input.size());
  for (size_t i = 0; i < f->type_->input.size(); ++i) {
    arg_regs.push_back(
        type::Apply(f->type_->input[i], [&](auto type_holder) -> Reg {
          using T = typename decltype(type_holder)::type;
          return MakeReg(arguments.results().get<T>(i));
        }));
  }

  BlockIndex start(CompiledFn::Current->blocks_.size());

  for (size_t i = 1; i < f->blocks_.size(); ++i) {
    auto &block = CompiledFn::Current->block(CompiledFn::AddBlock());
    block       = f->blocks_.at(i);
    block.cmd_buffer_.UpdateForInlining(inliner);
  }

  auto &block = CompiledFn::Current->block(BasicBlock::Current);

  UncondJump(start);
  BasicBlock::Current = inliner.landing();

  size_t i = 0;
  for (auto const &block : CompiledFn::Current->blocks_) {
    DEBUG_LOG("str")(i, ": ", block.cmd_buffer_.to_string());
    i++;
  }

  inliner.MergeAllocations(CompiledFn::Current, f->allocs());
  for (auto const &cmd : f->block(f->entry()).cmds_) {
    switch (cmd->op_code_) {
      default: UNREACHABLE();
    }
  }

  Results results;
  for (auto const &r : return_vals) { results.append(r); }
  return std::pair{results, is_jump};
}

}  // namespace ir
