#ifndef ICARUS_IR_INLINER_H
#define ICARUS_IR_INLINER_H

#include "ir/blocks/basic.h"
#include "ir/blocks/group.h"
#include "ir/builder.h"
#include "ir/jump.h"
#include "ir/local_block_interpretation.h"
#include "ir/results.h"

namespace ir {

// Inlines a `Jump` into the corresponding target block group (function or
// jump). Returns a mapping from the name of a block being jumped to, to the
// newly allocated BasicBlock that we use as the corresponding landing pad.
absl::flat_hash_map<
    std::string_view,
    std::pair<BasicBlock *, core::FnArgs<type::Typed<ir::Results>>>>
Inline(Builder &bldr, Jump *to_be_inlined,
       absl::Span<ir::Results const> arguments,
       LocalBlockInterpretation const &);

}  // namespace ir

#endif  // ICARUS_IR_INLINER_H
