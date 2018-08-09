#ifndef ICARUS_IR_BASIC_BLOCK_H
#define ICARUS_IR_BASIC_BLOCK_H
#include <list>
#include "base/container/vector.h"
#include "base/untyped_buffer.h"

#include "cmd.h"

namespace IR {
struct Func;

struct BasicBlock {
  static BlockIndex Current;
  BasicBlock()                    = delete;
  BasicBlock(const BasicBlock &&) = delete;
  BasicBlock(BasicBlock &&)       = default;
  BasicBlock(Func *fn) : fn_(fn) {}

  BasicBlock &operator=(BasicBlock &&) = default;
  BasicBlock &operator=(const BasicBlock &) = delete;

  void dump(size_t indent) const;

  Func *fn_;  // Containing function
  base::vector<BlockIndex> incoming_blocks_;
  base::vector<Cmd> cmds_;

  // These containers are append-only and we separately store pointers to these
  // elments so we never traverse. We just need pointer stabiltiy. In the long
  // term a single allocation is probably better but that's not easy with the
  // current setup.
  std::list<base::vector<IR::Val>> call_args_;
  std::list<LongArgs> long_args_;
  std::list<OutParams> outs_;
  std::vector<std::unique_ptr<GenericPhiArgs>> phi_args_;
};
}  // namespace IR
#endif  // ICARUS_IR_BASIC_BLOCK_H
