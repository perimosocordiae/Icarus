#include "compiler/compiler.h"

#include "ast/ast.h"
#include "ast/expr_ptr.h"
#include "backend/eval.h"
#include "compiler/compiler.h"
#include "compiler/module.h"
#include "frontend/parse.h"
#include "ir/builder.h"
#include "ir/compiled_fn.h"
#include "ir/jump_handler.h"
#include "ir/results.h"
#include "type/jump.h"

std::atomic<bool> found_errors = false;
std::atomic<ir::CompiledFn *> main_fn = nullptr;

namespace compiler {

static void CompileNodes(Compiler *compiler,
                         base::PtrSpan<ast::Node const> nodes) {
  for (ast::Node const *node : nodes) {
    compiler->Visit(node, VerifyTypeTag{});
  }
  if (compiler->num_errors() > 0) { return; }

  for (ast::Node const *node : nodes) { compiler->Visit(node, EmitValueTag{}); }
  compiler->CompleteDeferredBodies();
  if (compiler->num_errors() > 0) { return; }

  for (ast::Node const *node : nodes) {
    if (auto const *decl = node->if_as<ast::Declaration>()) {
      if (decl->id() != "main") { continue; }
      auto f = backend::EvaluateAs<ir::AnyFunc>(compiler->MakeThunk(
          decl->init_val(), compiler->type_of(decl->init_val())));
      ASSERT(f.is_fn() == true);
      auto ir_fn = f.func();

      // TODO check more than one?

      main_fn = ir_fn;
    } else {
      continue;
    }
  }
}

std::unique_ptr<module::BasicModule> CompileModule(frontend::Source *src) {
  auto mod = std::make_unique<CompiledModule>(
      [](base::PtrSpan<ast::Node const> nodes, CompiledModule *mod) {
        compiler::Compiler c(mod);
        CompileNodes(&c, nodes);
        mod->dep_data_   = std::move(c.data_.dep_data_);
        mod->fns_        = std::move(c.data_.fns_);
        mod->scope_defs_ = std::move(c.data_.scope_defs_);
        mod->block_defs_ = std::move(c.data_.block_defs_);
      });
  mod->Process(frontend::Parse(src));
  // TODO mark found_errors any were found
  return mod;
}

Compiler::Compiler(module::BasicModule *mod) : data_(mod) {}

VerifyResult const *Compiler::prior_verification_attempt(ast::ExprPtr expr) {
  return data_.constants_->second.result(expr);
}

type::Type const *Compiler::type_of(ast::Expression const *expr) const {
  if (auto *decl = expr->if_as<ast::Declaration>()) {
    if (auto *t = data_.current_constants_.type_of(decl)) { return t; }
  }

  auto *result = data_.constants_->second.result(expr);
  if (result and result->type()) { return result->type(); }

  // TODO reenabel once modules are all in core.
  // // When searching in embedded modules we intentionally look with no bound
  // // constants. Across module boundaries, a declaration can't be present
  // anyway. for (module::BasicModule const *mod :
  // mod_->scope_.embedded_modules_) {
  //   // TODO use right constants
  //   if (auto iter = mod->dep_data_.front().second.verify_results_.find(expr);
  //       iter != mod->dep_data_.front().second.verify_results_.end()) {
  //     return iter->second.type_;
  //   }
  // }
  return nullptr;
}

void Compiler::set_addr(ast::Declaration const *decl, ir::Reg addr) {
  data_.constants_->second.addr_[decl] = addr;
}
VerifyResult Compiler::set_result(ast::ExprPtr expr, VerifyResult r) {
  return data_.constants_->second.set_result(expr, r);
}

ir::Reg Compiler::addr(ast::Declaration const *decl) const {
  return data_.constants_->second.addr_.at(decl);
}

void Compiler::set_dispatch_table(ast::ExprPtr expr,
                                  ast::DispatchTable &&table) {
  // TODO data_.constants_->second.dispatch_tables_.emplace(expr, std::move(table));
  // TODO in some situations you may be trying to set the dispatch table more
  // than once. This has come up with generic structs and you should
  // investigate.
  //
  // static_cast<void>(iter);
  // ASSERT(success) << expr;
}

std::pair<ConstantBinding, DependentData> *Compiler::insert_constants(
    ConstantBinding const &constant_binding) {
  // TODO remove this iteration
  for (auto iter = data_.dep_data_.begin(); iter != data_.dep_data_.end(); ++iter) {
    auto &[key, val] = *iter;
    if (key == constant_binding) { return &*iter; }
  }
  auto *pair = &data_.dep_data_.emplace_front(constant_binding, DependentData{});
  pair->second.constants_ = pair->first;

  for (auto const &[decl, binding] : constant_binding.keys_) {
    pair->second.set_result(decl, VerifyResult::Constant(binding.type_));
  }
  return pair;
}

void Compiler::set_jump_table(ast::ExprPtr jump_expr, ast::ExprPtr node,
                              ast::DispatchTable &&table) {
  // TODO data_.constants_->second.jump_tables_.emplace(std::pair{jump_expr, node},
         //                                        std::move(table));
  // TODO in some situations you may be trying to set the dispatch table more
  // than once. This has come up with generic structs and you should
  // investigate.
  //
  // static_cast<void>(iter);
  // ASSERT(success) << expr;
}

void Compiler::set_pending_module(ast::Import const *import_node,
                                  module::PendingModule mod) {
  data_.constants_->second.imported_module_.emplace(import_node, std::move(mod));
}

ast::DispatchTable const *Compiler::dispatch_table(ast::ExprPtr expr) const {
  /* TODO auto &table = data_.constants_->second.dispatch_tables_;
  if (auto iter = table.find(expr); iter != table.end()) {
    return &iter->second;
  }*/
  return nullptr;
}

ast::DispatchTable const *Compiler::jump_table(ast::ExprPtr jump_expr,
                                               ast::ExprPtr node) const {
  /* TODOauto &table = data_.constants_->second.jump_tables_;
  if (auto iter = table.find(std::pair(jump_expr, node)); iter != table.end()) {
    return &iter->second;
  }*/
  return nullptr;
}

module::PendingModule *Compiler::pending_module(
    ast::Import const *import_node) const {
  if (auto iter = data_.constants_->second.imported_module_.find(import_node);
      iter != data_.constants_->second.imported_module_.end()) {
    return &iter->second;
  }
  return nullptr;
}

void Compiler::CompleteDeferredBodies() {
  base::move_func<void()> f;
  while (true) {
    {
      auto handle = data_.deferred_work_.lock();
      if (handle->empty()) { return; }
      auto nh = handle->extract(handle->begin());
      f       = std::move(nh.mapped());
    }
    std::move(f)();
  }
}

ir::CompiledFn *Compiler::AddFunc(
    type::Function const *fn_type,
    core::FnParams<type::Typed<ast::Declaration const *>> params) {
  return data_.fns_
      .emplace_back(
          std::make_unique<ir::CompiledFn>(fn_type, std::move(params)))
      .get();
}

ir::CompiledFn *Compiler::AddJump(
    type::Jump const *jump_type,
    core::FnParams<type::Typed<ast::Declaration const *>> params) {
  return data_.fns_
      .emplace_back(std::make_unique<ir::CompiledFn>(jump_type->ToFunction(),
                                                     std::move(params)))
      .get();
}

ir::CompiledFn Compiler::MakeThunk(ast::Expression const *expr,
                                   type::Type const *type) {
  ir::CompiledFn fn(type::Func({}, {ASSERT_NOT_NULL(type)}),
                    core::FnParams<type::Typed<ast::Declaration const *>>{});
  ICARUS_SCOPE(ir::SetCurrentFunc(&fn)) {
    // TODO this is essentially a copy of the body of FunctionLiteral::EmitValue
    // Factor these out together.
    builder().CurrentBlock() = fn.entry();

    auto vals = Visit(expr, compiler::EmitValueTag{});
    // TODO wrap this up into SetRet(vector)
    std::vector<type::Type const *> extracted_types;
    if (auto *tup = type->if_as<type::Tuple>()) {
      extracted_types = tup->entries_;
    } else {
      extracted_types = {type};
    }
    for (size_t i = 0; i < vals.size(); ++i) {
      ir::SetRet(i, type::Typed{vals.GetResult(i), extracted_types.at(i)});
    }
    builder().ReturnJump();

    CompleteDeferredBodies();
  }
  return fn;
}

}  // namespace compiler