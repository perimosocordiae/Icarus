#ifndef ICARUS_ARCHITECTURE_H
#define ICARUS_ARCHITECTURE_H

#include "ir/ir.h"
#include "type/type.h"
#include <cstddef>

// We only support architectures on which a byte is 8 bits, and assume all
// alignments are powers of two.
struct Architecture {
  size_t alignment(const Type *t) const;
  size_t bytes(const Type *t) const;

  size_t MoveForwardToAlignment(const Type *t, size_t index) const {
    return ((index - 1) | (alignment(t) - 1)) + 1;
  }

  // TODO skip the last alignment requirement?
  IR::Val ComputeArrayLength(IR::Val len, const Type *t) const {
    auto space_in_array = MoveForwardToAlignment(t, bytes(t));
    return IR::Mul(len, IR::Val::Uint(space_in_array));
  }

  u64 ComputeArrayLength(u64 len, const Type *t) const {
    return len * MoveForwardToAlignment(t, bytes(t));
  }

  static constexpr Architecture InterprettingMachine() {
    return Architecture{sizeof(IR::Addr), alignof(IR::Addr)};
  }

  static constexpr Architecture CompilingMachine() {
    return Architecture{sizeof(void *), alignof(void *)};
  }

  size_t ptr_bytes_;
  size_t ptr_align_;
};

#endif // ICARUS_ARCHITECTURE_H
