#ifndef ICARUS_IR_COMPILED_FN_H
#define ICARUS_IR_COMPILED_FN_H

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "base/move_func.h"
#include "base/ptr_span.h"
#include "base/scope.h"
#include "core/fn_params.h"
#include "ir/basic_block.h"
#include "ir/builder.h"
#include "ir/inliner.h"
#include "ir/stack_frame_allocations.h"
#include "type/typed_value.h"

namespace ast {
struct Expression;
}  // namespace ast

namespace type {
struct Function;
}  // namespace type

namespace ir {
struct BlockDef;

struct CompiledFn {
  CompiledFn(type::Function const *fn_type,
             core::FnParams<type::Typed<ast::Expression const *>> params);

  Inliner inliner() {
    BasicBlock *block = blocks().back();
    blocks_.push_back(std::make_unique<BasicBlock>(this));
    return Inliner(compiler_reg_to_offset_.size(), blocks_.size() - 1, block);
  }

  std::string name() const;

  base::PtrSpan<BasicBlock const> blocks() const { return blocks_; }
  base::PtrSpan<BasicBlock> blocks() { return blocks_; }

  Reg Reserve(type::Type const *t);
  Reg Reserve(core::Bytes b, core::Alignment a);

  Reg Alloca(type::Type const *t);

  BasicBlock const *entry() const { return blocks()[0]; }
  BasicBlock *entry() { return blocks()[0]; }

  StackFrameAllocations const &allocs() { return allocs_; }

  type::Function const *const type_ = nullptr;
  core::FnParams<type::Typed<ast::Expression const *>> params_;

  int32_t num_regs_ = 0;
  std::vector<std::unique_ptr<BasicBlock>> blocks_;
  base::move_func<void()> *work_item = nullptr;

  core::Bytes reg_size_ = core::Bytes{0};

  // This vector is indexed by Reg and stores the value which is the offset
  // into the base::untyped_buffer holding all registers during compile-time
  // execution. It is only valid for core::Host().
  absl::flat_hash_map<Reg, size_t> compiler_reg_to_offset_;
  StackFrameAllocations allocs_;
  bool must_inline_ = false;

  // TODO this is a hack until you figure out how to handle scoping/jump
  // handlers/etc. correctly. For now, jump_handlers fill this out and regular
  // functions do not. Probably this means these two things should be separated
  // into different types.
  std::vector<BlockDef const *> jumps_;
};

static_assert(alignof(CompiledFn) > 1);

std::ostream &operator<<(std::ostream &, CompiledFn const &);

}  // namespace ir

#endif  // ICARUS_IR_COMPILED_FN_H
