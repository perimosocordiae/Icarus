#include "ast/function_literal.h"

#include <sstream>
#include "ast/bound_constants.h"
#include "ast/declaration.h"
#include "ast/stages.h"
#include "ast/terminal.h"
#include "ast/verify_macros.h"
#include "backend/eval.h"
#include "context.h"
#include "error/log.h"
#include "ir/func.h"
#include "module.h"
#include "type/function.h"
#include "type/tuple.h"
#include "type/type.h"

namespace AST {
std::string FunctionLiteral::to_string(size_t n) const {
  std::stringstream ss;
  ss << "(";
  if (!inputs.empty()) {
    auto iter = inputs.begin();
    ss << (*iter)->to_string(n);
    ++iter;
    while (iter != inputs.end()) {
      ss << ", " << (*iter)->to_string(n);
      ++iter;
    }
  }
  ss << ") -> ";
  if (!return_type_inferred_) {
    ss << "(";
    if (!outputs.empty()) {
      auto iter = outputs.begin();
      ss << (*iter)->to_string(n);
      ++iter;
      while (iter != outputs.end()) {
        ss << ", " << (*iter)->to_string(n);
        ++iter;
      }
    }
    ss << ")";
  }
  ss << " {\n"
     << statements->to_string(n + 1) << std::string(2 * n, ' ') << "}";
  return ss.str();
}

void FunctionLiteral::assign_scope(Scope *scope) {
  STAGE_CHECK(AssignScopeStage, AssignScopeStage);
  scope_ = scope;
  if (!fn_scope) {
    fn_scope         = scope->add_child<FnScope>();
    fn_scope->fn_lit = this;
  }
  for (auto &in : inputs) { in->assign_scope(fn_scope.get()); }
  for (auto &out : outputs) { out->assign_scope(fn_scope.get()); }
  statements->assign_scope(fn_scope.get());
}

type::Type const *FunctionLiteral::VerifyType(Context *ctx) {
  VERIFY_STARTING_CHECK_EXPR;

  base::vector<const type::Type *> input_type_vec;
  input_type_vec.reserve(inputs.size());
  for (auto &input : inputs) {
    input_type_vec.push_back(input->VerifyType(ctx));
    HANDLE_CYCLIC_DEPENDENCIES;
  }

  for (auto &output : outputs) {
    output->VerifyType(ctx);
    HANDLE_CYCLIC_DEPENDENCIES;
  }

  if (ctx->num_errors() > 0) {
    limit_to(StageRange::Nothing());
    return nullptr;
  }

  if (!return_type_inferred_) {
    bool err = false;
    // TODO should named return types be required?
    base::vector<IR::Val> out_vals;
    out_vals.reserve(outputs.size());

    for (const auto &out : outputs) {
      auto result = backend::Evaluate(
          [&]() {
            if (!out->is<Declaration>()) { return out.get(); }

            auto *out_decl = &out->as<Declaration>();
            if (out_decl->IsInferred()) { NOT_YET(); }
            return out_decl->type_expr.get();
          }(),
          ctx);
      ASSERT(result.size() == 1u);
      out_vals.push_back(std::move(result)[0]);
    }

    base::vector<const type::Type *> ret_types;
    ret_types.reserve(out_vals.size());

    for (size_t i = 0; i < out_vals.size(); ++i) {
      const auto &out = out_vals[i];
      if (out == IR::Val::None() /* TODO Error() */) {
        ctx->error_log_.IndeterminantType(outputs[i]->span);
        limit_to(StageRange::Nothing());
      } else if (out.type != type::Type_) {
        ctx->error_log_.NotAType(outputs[i].get());
        limit_to(StageRange::Nothing());
        continue;
      } else if (auto *ret_type = std::get<const type::Type *>(out.value);
                 ret_type == nullptr) {
        err = true;
        limit_to(StageRange::Nothing());
        continue;
      } else {
        ret_types.push_back(ret_type);
      }
    }
    if (err) { return nullptr; }
    auto *t = type::Func(std::move(input_type_vec), std::move(ret_types));
    ctx->mod_->set_type(ctx->bound_constants_, this, t);
    return t;
  } else {
    Validate(ctx);
    return ctx->type_of(this);
  }
}

// TODO VerifyType has access to types of previous entries, but Validate
// doesnt.
void FunctionLiteral::Validate(Context *ctx) {
  STAGE_CHECK(StartBodyValidationStage, DoneBodyValidationStage);
  for (auto &in : inputs) { in->Validate(ctx); }
  for (auto &out : outputs) { out->Validate(ctx); }

  // NOTE! Type verifcation on statements first!
  statements->VerifyType(ctx);
  [&]() {
    HANDLE_CYCLIC_DEPENDENCIES;
    return nullptr;
  }();

  base::vector<const Expression *> rets;
  statements->ExtractReturns(&rets);
  statements->Validate(ctx);
  std::set<const type::Type *> types;
  for (auto *expr : rets) { types.insert(ctx->type_of(expr)); }

  base::vector<const type::Type *> input_type_vec, output_type_vec;
  input_type_vec.reserve(inputs.size());
  for (const auto &input : inputs) {
    input_type_vec.push_back(ctx->type_of(input.get()));
  }

  if (return_type_inferred_) {
    switch (types.size()) {
      case 0:
        ctx->mod_->set_type(ctx->bound_constants_, this,
                            type::Func(std::move(input_type_vec), {}));
        break;
      case 1: {
        auto *one_type = *types.begin();
        if (one_type->is<type::Tuple>()) {
          const auto &entries = one_type->as<type::Tuple>().entries_;
          for (auto *entry : entries) {
            outputs.push_back(
                std::make_unique<Terminal>(TextSpan(), IR::Val(entry)));
          }
          ctx->mod_->set_type(ctx->bound_constants_, this,
                              type::Func(std::move(input_type_vec), entries));

        } else {
          outputs.push_back(
              std::make_unique<Terminal>(TextSpan(), IR::Val(one_type)));
          ctx->mod_->set_type(
              ctx->mod_->bound_constants_, this,
              type::Func(std::move(input_type_vec), {one_type}));
        }
      } break;
      default: {
        // Note: this feels impossible, but it is possible if we allow scopes to
        // both evaluate to values and return.
        NOT_YET();
      } break;
    }
  } else {
    const auto &outs = ctx->type_of(this)->as<type::Function>().output;
    switch (outs.size()) {
      case 0: {
        for (auto *expr : rets) {
          ctx->error_log_.NoReturnTypes(expr);
          limit_to(StageRange::NoEmitIR());
        }
      } break;
      case 1: {
        for (auto *expr : rets) {
          if (ctx->type_of(expr) == outs[0]) { continue; }
          limit_to(StageRange::NoEmitIR());
          ctx->error_log_.ReturnTypeMismatch(outs[0], expr);
        }
      } break;
      default: {
        for (auto *expr : rets) {
          auto *expr_type = ctx->type_of(expr);
          if (expr_type->is<type::Tuple>()) {
            const auto &tup_entries = expr_type->as<type::Tuple>().entries_;
            if (tup_entries.size() != outs.size()) {
              ctx->error_log_.ReturningWrongNumber(expr, outs.size());
              limit_to(StageRange::NoEmitIR());
            } else {
              for (size_t i = 0; i < tup_entries.size(); ++i) {
                // TODO compare with Join rather than direct comparison
                if (tup_entries.at(i) != outs.at(i)) {
                  // TODO if this is a commalist we can point to it more
                  // carefully but if we're just passing on multiple return
                  // values it's harder.
                  ctx->error_log_.IndexedReturnTypeMismatch(outs.at(i), expr,
                                                            i);
                  limit_to(StageRange::NoEmitIR());
                }
              }
            }
          } else {
            ctx->error_log_.ReturningWrongNumber(expr, outs.size());
            limit_to(StageRange::NoEmitIR());
          }
        }
      } break;
    }
  }
}

void FunctionLiteral::SaveReferences(Scope *scope,
                                     base::vector<IR::Val> *args) {
  for (auto &input : inputs) { input->SaveReferences(scope, args); }
  for (auto &output : outputs) { output->SaveReferences(scope, args); }
  statements->SaveReferences(fn_scope.get(), args);
}

void FunctionLiteral::contextualize(
    const Node *correspondant,
    const base::unordered_map<const Expression *, IR::Val> &replacements) {
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->contextualize(
        correspondant->as<FunctionLiteral>().inputs[i].get(), replacements);
  }
  for (size_t i = 0; i < outputs.size(); ++i) {
    outputs[i]->contextualize(
        correspondant->as<FunctionLiteral>().outputs[i].get(), replacements);
  }

  statements->contextualize(
      correspondant->as<FunctionLiteral>().statements.get(), replacements);
}

void FunctionLiteral::ExtractReturns(
    base::vector<const Expression *> *rets) const {
  for (auto &in : inputs) { in->ExtractReturns(rets); }
  for (auto &out : outputs) { out->ExtractReturns(rets); }
}

FunctionLiteral *FunctionLiteral::Clone() const {
  auto *result                  = new FunctionLiteral;
  result->module_               = module_;
  result->span                  = span;
  result->statements            = base::wrap_unique(statements->Clone());
  result->lookup_               = lookup_;
  result->return_type_inferred_ = return_type_inferred_;
  result->inputs.reserve(inputs.size());
  for (const auto &input : inputs) {
    result->inputs.emplace_back(input->Clone());
  }
  result->outputs.reserve(outputs.size());
  for (const auto &output : outputs) {
    result->outputs.emplace_back(output->Clone());
  }

  result->ir_func_ = nullptr;
  return result;
}

base::vector<IR::Val> FunctionLiteral::EmitIR(Context *ctx) {
  if (stage_range_.high < EmitStage) { return {}; }

  if (!ir_func_) {
    base::vector<std::pair<std::string, Expression *>> args;
    args.reserve(inputs.size());
    for (const auto &input : inputs) {
      args.emplace_back(input->as<Declaration>().identifier->token,
                        input->as<Declaration>().init_val.get());
    }

    ir_func_ = ctx->mod_->AddFunc(
        this, &ctx->type_of(this)->as<type::Function>(), std::move(args));
    ctx->mod_->to_complete_.push(this);
  }

  return {IR::Val::Func(ir_func_)};
}

void FunctionLiteral::CompleteBody(Module *mod) {
  if (completed_) { return; }
  completed_ = true;

  Context ctx(mod);
  statements->VerifyType(&ctx);
  statements->Validate(&ctx);
  limit_to(statements);
  stage_range_.low = EmitStage;

  if (stage_range_.high < EmitStage) { return; }
  auto *t = ctx.type_of(this);
  if (t == type::Err) { return; }

  CURRENT_FUNC(ir_func_) {
    IR::BasicBlock::Current = ir_func_->entry();
    // Leave space for allocas that will come later (added to the entry
    // block).
    auto start_block        = IR::Func::Current->AddBlock();
    IR::BasicBlock::Current = start_block;

    // TODO arguments should be renumbered to not waste space on const values
    for (i32 i = 0; i < static_cast<i32>(inputs.size()); ++i) {
      inputs[i]->addr_ = IR::Func::Current->Argument(i);
    }

    for (size_t i = 0; i < outputs.size(); ++i) {
      if (!outputs[i]->is<Declaration>()) { continue; }
      outputs[i]->as<Declaration>().addr_ = IR::Func::Current->Return(i);
    }

    fn_scope->MakeAllStackAllocations(mod);

    statements->EmitIR(&ctx);
    if (t->as<type::Function>().output.empty()) {
      // TODO even this is wrong. Figure out the right jumping strategy
      // between here and where you call SetReturn
      IR::ReturnJump();
    }

    IR::BasicBlock::Current = ir_func_->entry();
    IR::UncondJump(start_block);
  }
}

base::vector<IR::Register> FunctionLiteral::EmitLVal(Context *) {
  UNREACHABLE(this);
}
}  // namespace AST
