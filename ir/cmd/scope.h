#ifndef ICARUS_IR_CMD_SCOPE_H
#define ICARUS_IR_CMD_SCOPE_H

#include "absl/types/span.h"
#include "ir/block.h"
#include "ir/cmd/util.h"

namespace ir {

struct BlockCmd {
  constexpr static cmd_index_t index = 39;

  static BasicBlock const *Execute(base::untyped_buffer::const_iterator *iter,
                                   std::vector<Addr> const &ret_slots,
                                   backend::ExecContext *ctx);

  static std::string DebugString(base::untyped_buffer::const_iterator *iter);

  static void UpdateForInlining(base::untyped_buffer::iterator *iter,
                                Inliner const &inliner);

};

struct ScopeCmd {
  constexpr static cmd_index_t index = 40;

  static BasicBlock const *Execute(base::untyped_buffer::const_iterator *iter,
                                   std::vector<Addr> const &ret_slots,
                                   backend::ExecContext *ctx);

  static std::string DebugString(base::untyped_buffer::const_iterator *iter);

  static void UpdateForInlining(base::untyped_buffer::iterator *iter,
                                Inliner const &inliner);

};

// TODO "Handler" doesn't really make sense in the name for these.
Reg BlockHandler(absl::Span<RegOr<AnyFunc> const> befores,
                 absl::Span<RegOr<AnyFunc> const> afters);

Reg ScopeHandler(Module *mod, absl::Span<RegOr<AnyFunc> const> inits,
                 absl::Span<RegOr<AnyFunc> const> dones,
                 absl::flat_hash_map<std::string_view, ir::Reg> const &blocks);

}  // namespace ir
#endif  // ICARUS_IR_CMD_SCOPE_H
