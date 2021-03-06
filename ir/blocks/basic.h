#ifndef ICARUS_IR_BLOCKS_BASIC_H
#define ICARUS_IR_BLOCKS_BASIC_H

#include <iostream>
#include <memory>
#include <vector>

#include "base/meta.h"
#include "base/untyped_buffer.h"
#include "ir/blocks/load_store_cache.h"
#include "ir/blocks/offset_cache.h"
#include "ir/instruction/base.h"
#include "ir/instruction/jump.h"
#include "ir/instruction/op_codes.h"
#include "ir/value/addr.h"
#include "ir/value/reg_or.h"
#include "ir/value/value.h"

namespace ir {

// BasicBlock:
//
// A basic block is an ordered collection of instructions with no branches. When
// executed, each instruction will execute in order. `BasicBlock`s also have a
// jump describing which basic block to execute next.
struct BasicBlock {
  struct DebugInfo {
    // Description of the block.
    std::string header;

    // This is often zero but will be set if this block was added because it
    // came from a `jump() { ... }` that got inlined. Two blocks with non-zero
    // values in this field hold the same value if and only if they were
    // produced by the same inlining.
    uint64_t cluster_index = 0;
  };

  BasicBlock() = default;
  BasicBlock(BasicBlock const &b) noexcept;
  BasicBlock(DebugInfo dbg) noexcept : debug_(std::move(dbg)) {}
  BasicBlock(BasicBlock const &b, DebugInfo dbg) noexcept : BasicBlock(b) {
    debug_ = std::move(dbg);
  }
  BasicBlock(BasicBlock &&) noexcept;
  BasicBlock &operator=(BasicBlock const &) noexcept;
  BasicBlock &operator=(BasicBlock &&) noexcept;

  void ReplaceJumpTargets(BasicBlock *old_target, BasicBlock *new_target);
  void Append(BasicBlock &&b);

  // All `BasicBlocks` which can jump to this one. Some may jump unconditionally
  // whereas others may jump only conditionally.
  auto const &incoming() const { return incoming_; }

  constexpr DebugInfo const &debug() const { return debug_; }

  void insert_incoming(BasicBlock *b) {
    incoming_.insert(b);
    b->jump_ = JumpCmd::Uncond(this);
  }

  LoadStoreCache &load_store_cache() { return load_store_cache_; }
  OffsetCache &offset_cache() { return offset_cache_; }

  void erase_incoming(BasicBlock *b) { incoming_.erase(b); }

  absl::Span<Inst> instructions() { return absl::MakeSpan(instructions_); }
  absl::Span<Inst const> instructions() const { return instructions_; }

  void Append(VoidReturningInstruction auto inst) {
    instructions_.push_back(Inst{std::move(inst)});
  }

  Reg Append(ReturningInstruction auto inst) {
    Reg r = inst.result;
    instructions_.push_back(Inst{std::move(inst)});
    return r;
  }

  void set_jump(JumpCmd j) { jump_ = std::move(j); }
  JumpCmd const &jump() const { return jump_; }

  friend std::ostream &operator<<(std::ostream &os, BasicBlock const &b);

 private:
  friend struct ir::InstructionInliner;

  void RemoveOutgoingJumps();
  void AddOutgoingJumps(JumpCmd const &jump);
  void ExchangeJumps(BasicBlock const *b);

  std::vector<Inst> instructions_;

  LoadStoreCache load_store_cache_;
  OffsetCache offset_cache_;
  absl::flat_hash_set<BasicBlock *> incoming_;

  JumpCmd jump_ = JumpCmd::Unreachable();

  DebugInfo debug_;
};

}  // namespace ir

#endif  // ICARUS_IR_BLOCKS_BASIC_H
