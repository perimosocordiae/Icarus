#include "ast/unop.h"

#include "ast/fn_args.h"
#include "ast/terminal.h"
#include "ast/verify_macros.h"
#include "base/check.h"
#include "context.h"
#include "ir/func.h"
#include "type/all.h"

std::vector<IR::Val> Evaluate(AST::Expression *expr, Context *ctx);

std::vector<IR::Val> EmitCallDispatch(
    const AST::FnArgs<std::pair<AST::Expression *, IR::Val>> &args,
    const AST::DispatchTable &dispatch_table, const type::Type *ret_type,
    Context *ctx);

IR::Val PtrCallFix(const IR::Val& v);

void ForEachExpr(AST::Expression *expr,
                 const std::function<void(size_t, AST::Expression *)> &fn);

namespace AST {
using base::check::Is; 
using base::check::Not; 

std::vector<Expression *> FunctionOptions(const std::string &token,
                                          Scope *scope, Context *ctx);

const type::Type *SetDispatchTable(const FnArgs<Expression *> &args,
                                   std::vector<Expression *> fn_options,
                                   AST::DispatchTable *dispatch_table,
                                   Context *ctx);

std::string Unop::to_string(size_t n) const {
  std::stringstream ss;
  switch (op) {
    case Language::Operator::Return: ss << "return "; break;
    case Language::Operator::Print: ss << "print "; break;
    case Language::Operator::Free: ss << "free "; break;
    case Language::Operator::Mul: ss << "*"; break;
    case Language::Operator::And: ss << "&"; break;
    case Language::Operator::Sub: ss << "-"; break;
    case Language::Operator::Generate: ss << "generate "; break;
    case Language::Operator::Not: ss << "!"; break;
    case Language::Operator::At: ss << "@"; break;
    case Language::Operator::Eval: ss << "$"; break;
    case Language::Operator::Dots: ss << ".."; break;
    case Language::Operator::Ref: ss << "\\"; break;
    case Language::Operator::Needs: ss << "needs "; break;
    case Language::Operator::Ensure: ss << "ensure "; break;
    case Language::Operator::TypeOf: ss << ":? "; break;
    case Language::Operator::Pass: break;
    default: { UNREACHABLE(); }
  }

  ss << operand->to_string(n);
  return ss.str();
}

void Unop::assign_scope(Scope *scope) {
  STAGE_CHECK(AssignScopeStage, AssignScopeStage);
  scope_ = scope;
  operand->assign_scope(scope);
}

void Unop::ClearIdDecls() {
  stage_range_ = StageRange{};
  operand->ClearIdDecls();
}

void Unop::Validate(Context *ctx) {
  STAGE_CHECK(StartBodyValidationStage, DoneBodyValidationStage);
  operand->Validate(ctx);
}

void Unop::SaveReferences(Scope *scope, std::vector<IR::Val> *args) {
  if (op == Language::Operator::Ref) {
    // TODO need to extract the right module here
    Context ctx(nullptr);
    operand->assign_scope(scope);
    operand->VerifyType(&ctx);
    operand->Validate(&ctx);
    auto val = operand->EmitIR(&ctx);

    args->push_back(val);
    args->push_back(IR::Val::Ref(this));
  } else {
    operand->SaveReferences(scope, args);
  }
}

void Unop::contextualize(
    const Node *correspondant,
    const std::unordered_map<const Expression *, IR::Val> &replacements) {
  if (op == Language::Operator::Ref) {
    auto iter = replacements.find(&correspondant->as<Unop>());
    ASSERT(iter != replacements.end());
    auto terminal    = std::make_unique<Terminal>();
    terminal->scope_ = scope_; // TODO Eh? Do I care?
    terminal->span   = span;
    terminal->lvalue = lvalue; // TODO????
    terminal->type   = iter->second.type;
    terminal->value  = iter->second;
    operand          = std::move(terminal);
    op               = Language::Operator::Pass;
  } else {
    operand->contextualize(correspondant->as<Unop>().operand.get(),
                           replacements);
  }
}

void Unop::ExtractReturns(std::vector<const Expression *> *rets) const {
  operand->ExtractReturns(rets);
  if (op == Language::Operator::Return) { rets->push_back(operand.get()); }
}

Unop *Unop::Clone() const {
  auto *result    = new Unop;
  result->span    = span;
  result->operand = base::wrap_unique(operand->Clone());
  result->op      = op;
  return result;
}

void Unop::VerifyType(Context *ctx) {
  VERIFY_STARTING_CHECK_EXPR;
  VERIFY_AND_RETURN_ON_ERROR(operand);

  using Language::Operator;

  if (op != Operator::At && op != Operator::And) {
    lvalue = operand->lvalue == Assign::Const ? Assign::Const : Assign::LVal;
  }

  switch (op) {
    case Operator::TypeOf:
      type   = type::Type_;
      lvalue = Assign::Const;
      break;
    case Operator::Eval: type = operand->type; break;
    case Operator::Generate: type = type::Void; break;
    case Operator::Free: {
      if (!operand->type->is<type::Pointer>()) {
        ctx->error_log_.FreeingNonPointer(operand->type, span);
        limit_to(StageRange::NoEmitIR());
      }
      type = type::Void;
    } break;
    case Operator::Print: {
      if (operand->type == type::Void) {
        ctx->error_log_.PrintingVoid(span);
        limit_to(StageRange::NoEmitIR());
      }
      type = type::Void;
    } break;
    case Operator::Return: {
      if (operand->type == type::Void) {
        // TODO this should probably be allowed (c.f., regular void)
        ctx->error_log_.ReturningVoid(span);
        limit_to(StageRange::NoEmitIR());
      }
      type = type::Void;
    } break;
    case Operator::At: {
      lvalue = Assign::LVal;
      if (operand->type->is<type::Pointer>()) {
        type = operand->type->as<type::Pointer>().pointee;

      } else {
        ctx->error_log_.DereferencingNonPointer(operand->type, span);
        type = type::Err;
        limit_to(StageRange::Nothing());
      }
    } break;
    case Operator::And: {
      switch (operand->lvalue) {
        case Assign::Const:
          ctx->error_log_.TakingAddressOfConstant(span);
          lvalue = Assign::RVal;
          break;
        case Assign::RVal:
          ctx->error_log_.TakingAddressOfTemporary(span);
          lvalue = Assign::RVal;
          break;
        case Assign::LVal: break;
        case Assign::Unset: UNREACHABLE();
      }
      type = Ptr(operand->type);
    } break;
    case Operator::Mul: {
      limit_to(operand);
      if (operand->type != type::Type_) {
        ErrorLog::LogGeneric(
            this->span, "TODO " __FILE__ ":" + std::to_string(__LINE__) + ": ");
        type = type::Err;
        limit_to(StageRange::Nothing());
      } else {
        type = type::Type_;
      }
    } break;
    case Operator::Sub: {
      if (operand->type == type::Int || operand->type == type::Real) {
        type = operand->type;

      } else if (operand->type->is<type::Struct>()) {
        FnArgs<Expression *> args;
        args.pos_ = std::vector{operand.get()};
        type      = SetDispatchTable(args, FunctionOptions("-", scope_, ctx),
                                &dispatch_table_, ctx);
        ASSERT(type, Not(Is<type::Tuple>()));
        if (type == type::Err) { limit_to(StageRange::Nothing()); }
      }
    } break;
    case Operator::Dots: NOT_YET();
    case Operator::Not: {
      if (operand->type == type::Bool) {
        type = type::Bool;
      } else if (operand->type->is<type::Struct>()) {
        FnArgs<Expression *> args;
        args.pos_ = std::vector{operand.get()};
        type      = SetDispatchTable(args, FunctionOptions("!", scope_, ctx),
                                &dispatch_table_, ctx);
        ASSERT(type, Not(Is<type::Tuple>()));
        if (type == type::Err) { limit_to(StageRange::Nothing()); }
      } else {
        ErrorLog::LogGeneric(
            TextSpan(), "TODO " __FILE__ ":" + std::to_string(__LINE__) + ": ");

        type = type::Err;
        limit_to(StageRange::Nothing());
      }
    } break;
    case Operator::Needs: {
      type = type::Void;
      if (operand->type != type::Bool) {
        ctx->error_log_.PreconditionNeedsBool(this);
        limit_to(StageRange::NoEmitIR());
      }
    } break;
    case Operator::Ensure: {
      type = type::Void;
      if (operand->type != type::Bool) {
        ctx->error_log_.PostconditionNeedsBool(this);
        limit_to(StageRange::NoEmitIR());
      }
    } break;
    case Operator::Pass: type = operand->type; break;
    default: UNREACHABLE(*this);
  }
  limit_to(operand);
}

IR::Val Unop::EmitIR(Context *ctx) {
  if (operand->type->is<type::Struct>() && dispatch_table_.total_size_ != 0) {
    // TODO struct is not exactly right. we really mean user-defined
    FnArgs<std::pair<Expression *, IR::Val>> args;
    args.pos_    = {std::pair(operand.get(), operand->type->is_big()
                                              ? PtrCallFix(operand->EmitIR(ctx))
                                              : operand->EmitIR(ctx))};
    auto results = EmitCallDispatch(args, dispatch_table_, type, ctx);
    ASSERT(results.size() == 1u);
    return results[0];
  }

  switch (op) {
    case Language::Operator::Not:
    case Language::Operator::Sub: {
      return IR::Neg(operand->EmitIR(ctx));
    } break;
    case Language::Operator::Return: {
      if (!operand->type->is_big()) {
        IR::SetReturn(IR::ReturnValue{static_cast<i32>(0)},
                      operand->EmitIR(ctx));
      } else {
        ForEachExpr(operand.get(), [ctx](size_t i, AST::Expression *expr) {
          // TODO return type maybe not the same as type actually returned?
          auto *ret_type = expr->type;
          ret_type->EmitAssign(
              expr->type, expr->EmitIR(ctx),
              IR::Val::Ret(IR::ReturnValue{static_cast<i32>(i)},
                           ret_type->is_big() ? Ptr(ret_type) : ret_type),
              ctx);
        });
      }
      IR::ReturnJump();
      return IR::Val::None();
    }
    case Language::Operator::TypeOf: return IR::Val::Type(operand->type);
    case Language::Operator::Print: {
      ForEachExpr(operand.get(), [&ctx](size_t, AST::Expression *expr) {
        if (expr->type->is<type::Primitive>() ||
            expr->type->is<type::Pointer>()) {
          IR::Print(expr->EmitIR(ctx));
        } else {
          expr->type->EmitRepr(expr->EmitIR(ctx), ctx);
        }
      });

      return IR::Val::None();
    } break;
    case Language::Operator::And: return operand->EmitLVal(ctx);
    case Language::Operator::Eval: {
      // TODO what if there's an error during evaluation?
      // TODO what about ``a, b = $FnWithMultipleReturnValues()``
      auto results = Evaluate(operand.get(), ctx);
      return results.empty() ? IR::Val::None() : results[0];
    }
    case Language::Operator::Generate: {
      auto val = Evaluate(operand.get(), ctx) AT(0);
      ASSERT(val.type == type::Code);
      auto block = std::get<AST::CodeBlock>(val.value);
      if (auto *err = std::get_if<std::string>(&block.content_)) {
        ctx->error_log_.UserDefinedError(*err);
        return IR::Val::None();
      }

      auto *stmts = &std::get<AST::Statements>(block.content_);
      stmts->assign_scope(scope_);
      stmts->VerifyType(ctx);
      stmts->Validate(ctx);
      return stmts->EmitIR(ctx);

    } break;
    case Language::Operator::Mul: return IR::Ptr(operand->EmitIR(ctx));
    case Language::Operator::At: return PtrCallFix(operand->EmitIR(ctx));
    case Language::Operator::Needs: {
      // TODO validate requirements are well-formed?
      IR::Func::Current->preconditions_.push_back(operand.get());
      return IR::Val::None();
    } break;
    case Language::Operator::Ensure: {
      // TODO validate requirements are well-formed?
      IR::Func::Current->postconditions_.push_back(operand.get());
      return IR::Val::None();
    } break;
    case Language::Operator::Pass: return operand->EmitIR(ctx);
    default: UNREACHABLE("Operator is ", static_cast<int>(op));
  }
}

IR::Val Unop::EmitLVal(Context *ctx) {
  ASSERT(op == Language::Operator::At);
  return operand->EmitIR(ctx);
}
}  // namespace AST