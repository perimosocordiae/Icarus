#ifndef ICARUS_CONTEXT_H
#define ICARUS_CONTEXT_H

#include <vector>
#include <memory>

#include "ast/bound_constants.h"
#include "error/log.h"

struct Module;
struct Context {
  Context(Module* mod);
  
  size_t num_errors() { return error_log_.size(); }
  void DumpErrors() { error_log_.Dump(); }

  error::Log error_log_;
  const AST::BoundConstants *bound_constants_ = nullptr;
  Module *mod_                                = nullptr;

  // During validation, when a cyclic dependency is encountered, we write it
  // down here. That way, we can bubble up from the dependency until we see it
  // again, at each step adding the nodes to the error log involved in the
  // dependency. Once complete, we reset this to null
  std::vector<AST::Identifier *> *cyc_dep_vec_ = nullptr;
};

#endif // ICARUS_CONTEXT_H