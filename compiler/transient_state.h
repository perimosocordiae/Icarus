#ifndef ICARUS_COMPILER_TRANSIENT_STATE_H
#define ICARUS_COMPILER_TRANSIENT_STATE_H

#include <iterator>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "ast/ast.h"
#include "ir/blocks/basic.h"
#include "ir/instruction/core.h"
#include "ir/value/label.h"

namespace compiler {

struct WorkItem {
  enum class Result { Success, Failure, Deferred };
  enum class Kind {
    VerifyBlockBody,
    VerifyEnumBody,
    VerifyFunctionBody,
    VerifyJumpBody,
    VerifyScopeBody,
    VerifyStructBody,
    CompleteStructMembers,
  };

  Result Process() const;

  Kind kind;
  ast::Node const* node;
  // TODO: Should we just store PersistentResources directly?
  DependentComputedData &context;
  diagnostic::DiagnosticConsumer& consumer;
};


struct WorkQueue {
  bool empty() const { return items_.empty(); }

  void Enqueue(WorkItem item) { items_.push(std::move(item)); }

  void ProcessOneItem();

 private:
  bool Process(WorkItem const &item);

  std::queue<WorkItem> items_;

#if defined(ICARUS_DEBUG)
  size_t cycle_breaker_count_ = 0;
#endif
};

// Compiler state that needs to be tracked during the compilation of a single
// function or jump, but otherwise does not need to be saved.
struct TransientState {
  struct ScopeLandingState {
    ir::Label label;
    ir::BasicBlock *block;
    ir::PhiInstruction<int64_t> *phi;
  };
  std::vector<ScopeLandingState> scope_landings;

  WorkQueue work_queue;

  struct YieldedArguments {
    core::FnArgs<std::pair<ir::Value, type::QualType>> vals;
    ir::Label label;
  };
  std::vector<YieldedArguments> yields;

  bool must_complete = true;
};

}  // namespace compiler

#endif  // ICARUS_COMPILER_TRANSIENT_STATE_H