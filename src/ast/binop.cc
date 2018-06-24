#include "ast/binop.h"

#include "ast/comma_list.h"
#include "ast/fn_args.h"
#include "ast/verify_macros.h"
#include "backend/eval.h"
#include "base/check.h"
#include "context.h"
#include "ir/func.h"
#include "type/array.h"
#include "type/char_buffer.h"
#include "type/enum.h"
#include "type/flags.h"
#include "type/function.h"
#include "type/pointer.h"
#include "type/struct.h"
#include "type/tuple.h"

IR::Val PtrCallFix(const IR::Val& v);

namespace type {
const Pointer *Ptr(const Type *);
}  // namespace type

std::vector<IR::Val> EmitCallDispatch(
    const AST::FnArgs<std::pair<AST::Expression *, IR::Val>> &args,
    const AST::DispatchTable &dispatch_table, const type::Type *ret_type,
    Context *ctx);

// TODO move this to some weird util lib?
void ForEachExpr(AST::Expression *expr,
                 const std::function<void(size_t, AST::Expression *)> &fn) {
  if (expr->is<AST::CommaList>()) {
    const auto &exprs = expr->as<AST::CommaList>().exprs;
    for (size_t i = 0; i < exprs.size(); ++i) { fn(i, exprs[i].get()); }
  } else {
    fn(0, expr);
  }
}

namespace AST {
using base::check::Not;
using base::check::Is;

std::string Binop::to_string(size_t n) const {
  std::stringstream ss;
  if (op == Language::Operator::Index) {
    ss << lhs->to_string(n) << "[" << rhs->to_string(n) << "]";
    return ss.str();
  }

  ss << "(" << lhs->to_string(n) << ")";
  switch (op) {
  case Language::Operator::Arrow: ss << " -> "; break;
  case Language::Operator::Add: ss << " + "; break;
  case Language::Operator::Sub: ss << " - "; break;
  case Language::Operator::Mul: ss << " * "; break;
  case Language::Operator::Div: ss << " / "; break;
  case Language::Operator::Mod: ss << " % "; break;
  case Language::Operator::Assign: ss << " <<:=>> "; break;
  case Language::Operator::OrEq: ss << " |= "; break;
  case Language::Operator::XorEq: ss << " ^= "; break;
  case Language::Operator::AndEq: ss << " &= "; break;
  case Language::Operator::AddEq: ss << " += "; break;
  case Language::Operator::SubEq: ss << " -= "; break;
  case Language::Operator::MulEq: ss << " *= "; break;
  case Language::Operator::DivEq: ss << " /= "; break;
  case Language::Operator::ModEq: ss << " %= "; break;
  case Language::Operator::As: ss << " as "; break;
  case Language::Operator::When: ss << " when "; break;
  default: UNREACHABLE();
  }
  ss << "(" << rhs->to_string(n) << ")";

  return ss.str();
}

void Binop::assign_scope(Scope *scope) {
  STAGE_CHECK(AssignScopeStage, AssignScopeStage);
  scope_ = scope;
  lhs->assign_scope(scope);
  rhs->assign_scope(scope);
}

void Binop::VerifyType(Context *ctx) {
  VERIFY_STARTING_CHECK_EXPR;

  lhs->VerifyType(ctx);
  HANDLE_CYCLIC_DEPENDENCIES;
  rhs->VerifyType(ctx);
  HANDLE_CYCLIC_DEPENDENCIES;
  if (lhs->type == type::Err || rhs->type == type::Err) {
    type = type::Err;
    limit_to(lhs);
    limit_to(rhs);
    return;
  }

  using Language::Operator;
  if (lhs->lvalue != Assign::LVal && op == Operator::Assign) {
    switch (lhs->lvalue) {
      case Assign::Unset: UNREACHABLE();
      case Assign::Const: ctx->error_log_.AssigningToConstant(span); break;
      case Assign::RVal: ctx->error_log_.AssigningToTemporary(span); break;
      case Assign::LVal: UNREACHABLE();
    }
    limit_to(StageRange::Nothing());

  } else if (lhs->lvalue != Assign::LVal &&
             (op == Operator::OrEq || op == Operator::XorEq ||
              op == Operator::AndEq || op == Operator::AddEq ||
              op == Operator::SubEq || op == Operator::MulEq ||
              op == Operator::DivEq || op == Operator::ModEq)) {
    switch (lhs->lvalue) {
      case Assign::Unset: UNREACHABLE();
      case Assign::Const: ctx->error_log_.ModifyingToConstant(span); break;
      case Assign::RVal: ctx->error_log_.ModifyingToTemporary(span); break;
      case Assign::LVal: UNREACHABLE();
    }
    limit_to(StageRange::Nothing());
  } else if (op == Operator::Index) {
    lvalue = rhs->lvalue;
  } else if (lhs->lvalue == Assign::Const && rhs->lvalue == Assign::Const) {
    lvalue = Assign::Const;
  } else {
    lvalue = Assign::RVal;
  }

  // TODO if lhs is reserved?
  if (op == Operator::Assign) {
    if (lhs->type->is<type::Tuple>()) {
      if (rhs->type->is<type::Tuple>()) {
        const auto &lhs_entries_ = lhs->type->as<type::Tuple>().entries_;
        const auto &rhs_entries_ = rhs->type->as<type::Tuple>().entries_;

        if (lhs_entries_.size() != rhs_entries_.size()) {
          NOT_YET("error message");
        } else {
          for (size_t i = 0; i < lhs_entries_.size(); ++i) {
            if (!type::CanCastImplicitly(rhs_entries_[i], lhs_entries_[i])) {
              NOT_YET("log an error");
              limit_to(StageRange::NoEmitIR());
            }
          }
        }
      } else {
        LOG << lhs;
        LOG << rhs;
        NOT_YET("error message");
      }
    } else {
      if (rhs->type->is<type::Tuple>()){
        LOG << lhs;
        LOG << rhs;
        NOT_YET("error message");
      } else {
        if (!type::CanCastImplicitly(rhs->type, lhs->type)) {
          NOT_YET("log an error");
          limit_to(StageRange::NoEmitIR());
        }
      }
    }

    return;
  }

  switch (op) {
    case Operator::Index: {
      type = type::Err;
      if (lhs->type->is<type::CharBuffer>()) {
        if (rhs->type != type::Int) {
          ctx->error_log_.InvalidCharBufIndex(span, rhs->type);
          limit_to(StageRange::NoEmitIR());
        }
        type = type::Char;  // Assuming it's a char, even if the index type was
                            // wrong.
        return;
      } else if (!lhs->type->is<type::Array>()) {
        ctx->error_log_.IndexingNonArray(span, lhs->type);
        limit_to(StageRange::NoEmitIR());
        return;
      } else {
        type = lhs->type->as<type::Array>().data_type;

        if (rhs->type == type::Int) { break; }
        ctx->error_log_.NonIntegralArrayIndex(span, rhs->type);
        limit_to(StageRange::NoEmitIR());
        return;
      }
    } break;
    case Operator::As: {
      // TODO check that the type actually can be cast
      // correctly.
      type   = backend::EvaluateAs<const type::Type *>(rhs.get(), ctx);
      lvalue = lhs->lvalue;
    } break;
    case Operator::XorEq: {
      if (lhs->type == type::Bool && rhs->type == type::Bool) {
        type = type::Bool;
      } else if (lhs->type->is<type::Flags>() && rhs->type == lhs->type) {
        type = lhs->type;
      } else {
        type = type::Err;
        // TODO could be bool or enum.
        ctx->error_log_.XorEqNeedsBool(span);
        limit_to(StageRange::Nothing());
        return;
      }
    } break;
    case Operator::AndEq: {
      if (lhs->type == type::Bool && rhs->type == type::Bool) {
        type = type::Bool;
      } else if (lhs->type->is<type::Flags>() && rhs->type == lhs->type) {
        type = lhs->type;
      } else {
        type = type::Err;
        // TODO could be bool or enum.
        ctx->error_log_.AndEqNeedsBool(span);
        limit_to(StageRange::Nothing());
        return;
      }
    } break;
    case Operator::OrEq: {
      if (lhs->type == type::Bool && rhs->type == type::Bool) {
        type = type::Bool;
      } else if (lhs->type->is<type::Flags>() && rhs->type == lhs->type) {
        type = lhs->type;
      } else {
        type = type::Err;
        // TODO could be bool or enum.
        ctx->error_log_.OrEqNeedsBool(span);
        limit_to(StageRange::Nothing());
        return;
      }
    } break;

#define CASE(OpName, symbol, ret_type)                                         \
  case Operator::OpName: {                                                     \
    if ((lhs->type == type::Int && rhs->type == type::Int) ||                  \
        (lhs->type == type::Real && rhs->type == type::Real)) {                \
      type = ret_type;                                                         \
    } else {                                                                   \
      FnArgs<Expression *> args;                                               \
      args.pos_ = std::vector{lhs.get(), rhs.get()};                           \
      std::tie(dispatch_table_, type) =                                        \
          DispatchTable::Make(args, symbol, scope_, ctx);                      \
      ASSERT(type, Not(Is<type::Tuple>()));                                    \
      if (type == type::Err) {                                                 \
        ctx->error_log_.NoMatchingOperator(symbol, lhs->type, rhs->type,       \
                                           span);                              \
        limit_to(StageRange::Nothing());                                       \
      }                                                                        \
    }                                                                          \
  } break;

      CASE(Sub, "-", lhs->type);
      CASE(Div, "/", lhs->type);
      CASE(Mod, "%", lhs->type);
      CASE(SubEq, "-=", type::Void());
      CASE(MulEq, "*=", type::Void());
      CASE(DivEq, "/=", type::Void());
      CASE(ModEq, "%=", type::Void());
#undef CASE
    case Operator::Add: {
      if ((lhs->type == type::Int && rhs->type == type::Int) ||
          (lhs->type == type::Real && rhs->type == type::Real) ||
          (lhs->type == type::Code && rhs->type == type::Code)) {
        type = lhs->type;
      } else if (lhs->type->is<type::CharBuffer>() &&
                 rhs->type->is<type::CharBuffer>()) {
        type = type::CharBuf(lhs->type->as<type::CharBuffer>().length_ +
                             rhs->type->as<type::CharBuffer>().length_);
      } else {
        FnArgs<Expression *> args;
        args.pos_ = std::vector{lhs.get(), rhs.get()};
        std::tie(dispatch_table_, type) =
            DispatchTable::Make(args, "+", scope_, ctx);
        ASSERT(type, Not(Is<type::Tuple>()));
        if (type == type::Err) {
          ctx->error_log_.NoMatchingOperator("+", lhs->type, rhs->type, span);
          limit_to(StageRange::Nothing());
        }
      }
    } break;
    case Operator::AddEq: {
      if ((lhs->type == type::Int && rhs->type == type::Int) ||
          (lhs->type == type::Real && rhs->type == type::Real) ||
          (lhs->type == type::Code &&
           rhs->type == type::Code)) { /* TODO type::Code should only be valid
                                          for Add, not Sub, etc */
        type = type::Void();
      } else {
        FnArgs<Expression *> args;
        args.pos_ = std::vector{lhs.get(), rhs.get()};
        std::tie(dispatch_table_, type) =
            DispatchTable::Make(args, "+=", scope_, ctx);
        ASSERT(type, Not(Is<type::Tuple>()));
        if (type == type::Err) { limit_to(StageRange::Nothing()); }
      }
    } break;
    // Mul is done separately because of the function composition
    case Operator::Mul: {
      if ((lhs->type == type::Int && rhs->type == type::Int) ||
          (lhs->type == type::Real && rhs->type == type::Real)) {
        type = lhs->type;

      } else if (lhs->type->is<type::Function>() &&
                 rhs->type->is<type::Function>()) {
        auto *lhs_fn = &lhs->type->as<type::Function>();
        auto *rhs_fn = &rhs->type->as<type::Function>();
        if (rhs_fn->output == lhs_fn->input) {
          type = type::Func({rhs_fn->input}, {lhs_fn->output});

        } else {
          type = type::Err;
          ctx->error_log_.NonComposableFunctions(span);
          limit_to(StageRange::Nothing());
          return;
        }

      } else {
        FnArgs<Expression *> args;
        args.pos_  = std::vector{lhs.get(), rhs.get()};
        std::tie(dispatch_table_, type) =
            DispatchTable::Make(args, "*", scope_, ctx);
        ASSERT(type, Not(Is<type::Tuple>()));
        if (type == type::Err) {
          ctx->error_log_.NoMatchingOperator("+", lhs->type, rhs->type, span);
          limit_to(StageRange::Nothing());
        }
      }
    } break;
    case Operator::Arrow: {
      if (lhs->type != type::Type_) {
        type = type::Err;
        ctx->error_log_.NonTypeFunctionInput(span);
        limit_to(StageRange::Nothing());
        return;
      }
      if (rhs->type != type::Type_) {
        type = type::Err;
        ctx->error_log_.NonTypeFunctionOutput(span);
        limit_to(StageRange::Nothing());
        return;
      }

      if (type != type::Err) { type = type::Type_; }

    } break;
    default: UNREACHABLE();
  }
}

void Binop::Validate(Context *ctx) {
  STAGE_CHECK(StartBodyValidationStage, DoneBodyValidationStage);
  lhs->Validate(ctx);
  rhs->Validate(ctx);
}

void Binop::SaveReferences(Scope *scope, std::vector<IR::Val> *args) {
  lhs->SaveReferences(scope, args);
  rhs->SaveReferences(scope, args);
}

void Binop::contextualize(
    const Node *correspondant,
    const std::unordered_map<const Expression *, IR::Val> &replacements) {
  lhs->contextualize(correspondant->as<Binop>().lhs.get(), replacements);
  rhs->contextualize(correspondant->as<Binop>().rhs.get(), replacements);
}

void Binop::ExtractReturns(std::vector<const Expression *> *rets) const {
  lhs->ExtractReturns(rets);
  rhs->ExtractReturns(rets);
}

std::vector<IR::Val> AST::Binop::EmitIR(Context *ctx) {
  if (op != Language::Operator::Assign &&
      (lhs->type->is<type::Struct>() || rhs->type->is<type::Struct>())) {
    // TODO struct is not exactly right. we really mean user-defined
    AST::FnArgs<std::pair<AST::Expression *, IR::Val>> args;
    args.pos_.reserve(2);
    args.pos_.emplace_back(lhs.get(), PtrCallFix(lhs->EmitIR(ctx)[0]));
    args.pos_.emplace_back(rhs.get(), PtrCallFix(rhs->EmitIR(ctx)[0]));

    ASSERT(type != nullptr);
    return EmitCallDispatch(args, dispatch_table_, type, ctx);
  }

  switch (op) {
#define CASE(op_name)                                                          \
  case Language::Operator::op_name: {                                          \
    auto lhs_ir = lhs->EmitIR(ctx)[0];                                         \
    auto rhs_ir = rhs->EmitIR(ctx)[0];                                         \
    return {IR::op_name(lhs_ir, rhs_ir)};                                      \
  } break
    CASE(Add);
    CASE(Sub);
    CASE(Mul);
    CASE(Div);
    CASE(Mod);
#undef CASE
    case Language::Operator::As:
      return {IR::Cast(type, lhs->EmitIR(ctx)[0], ctx)};
    case Language::Operator::Arrow:
      return {IR::Arrow(IR::Tup(lhs->EmitIR(ctx)), IR::Tup(rhs->EmitIR(ctx)))};
    case Language::Operator::Assign: {
      std::vector<const type::Type *> lhs_types, rhs_types;
      ForEachExpr(rhs.get(), [&ctx, &rhs_types](size_t, Expression *expr) {
        if (expr->type->is<type::Tuple>()) {
          rhs_types.insert(rhs_types.end(),
                           expr->type->as<type::Tuple>().entries_.begin(),
                           expr->type->as<type::Tuple>().entries_.end());
        } else {
          rhs_types.push_back(expr->type);
        }
      });
      auto rhs_vals = rhs->EmitIR(ctx);

      // TODO types can be retrieved from the values?
      ForEachExpr(lhs.get(), [&ctx, &lhs_types](size_t, AST::Expression *expr) {
        lhs_types.push_back(expr->type);
      });
      auto lhs_lvals = lhs->EmitLVal(ctx);

      ASSERT(lhs_lvals.size() == rhs_vals.size());
      ASSERT(lhs_lvals.size() == lhs_types.size());
      ASSERT(lhs_lvals.size() == rhs_types.size());
      for (size_t i = 0; i < lhs_lvals.size(); ++i) {
        lhs_types[i]->EmitAssign(rhs_types[i], PtrCallFix(rhs_vals[i]),
                                 lhs_lvals[i], ctx);
      }
      return {};
    } break;
    case Language::Operator::OrEq: {
      if (type->is<type::Enum>()) {
        auto lhs_lval = lhs->EmitLVal(ctx)[0];
        IR::Store(IR::Or(IR::Load(lhs_lval), rhs->EmitIR(ctx)[0]), lhs_lval);
        return {};
      }
      auto land_block = IR::Func::Current->AddBlock();
      auto more_block = IR::Func::Current->AddBlock();

      auto lhs_val       = lhs->EmitIR(ctx)[0];
      auto lhs_end_block = IR::BasicBlock::Current;
      IR::CondJump(lhs_val, land_block, more_block);

      IR::BasicBlock::Current = more_block;
      auto rhs_val            = rhs->EmitIR(ctx)[0];
      auto rhs_end_block      = IR::BasicBlock::Current;
      IR::UncondJump(land_block);

      IR::BasicBlock::Current = land_block;

      auto phi = IR::Phi(type::Bool);
      IR::Func::Current->SetArgs(
          phi, {IR::Val::BasicBlock(lhs_end_block), IR::Val::Bool(true),
                IR::Val::BasicBlock(rhs_end_block), rhs_val});
      return {IR::Func::Current->Command(phi).reg()};
    } break;
    case Language::Operator::AndEq: {
      if (type->is<type::Enum>()) {
        auto lhs_lval = lhs->EmitLVal(ctx)[0];
        IR::Store(IR::And(IR::Load(lhs_lval), rhs->EmitIR(ctx)[0]), lhs_lval);
        return {};
      }

      auto land_block = IR::Func::Current->AddBlock();
      auto more_block = IR::Func::Current->AddBlock();

      auto lhs_val       = lhs->EmitIR(ctx)[0];
      auto lhs_end_block = IR::BasicBlock::Current;
      IR::CondJump(lhs_val, more_block, land_block);

      IR::BasicBlock::Current = more_block;
      auto rhs_val            = rhs->EmitIR(ctx)[0];
      auto rhs_end_block      = IR::BasicBlock::Current;
      IR::UncondJump(land_block);

      IR::BasicBlock::Current = land_block;

      auto phi = IR::Phi(type::Bool);
      IR::Func::Current->SetArgs(
          phi, {IR::Val::BasicBlock(lhs_end_block), IR::Val::Bool(false),
                IR::Val::BasicBlock(rhs_end_block), rhs_val});
      return {IR::Func::Current->Command(phi).reg()};
    } break;
#define CASE_ASSIGN_EQ(op_name)                                                \
  case Language::Operator::op_name##Eq: {                                      \
    auto lhs_lval = lhs->EmitLVal(ctx)[0];                                     \
    auto rhs_ir   = rhs->EmitIR(ctx)[0];                                       \
    IR::Store(IR::op_name(PtrCallFix(lhs_lval), rhs_ir), lhs_lval);            \
    return {};                                                                 \
  } break
      CASE_ASSIGN_EQ(Xor);
      CASE_ASSIGN_EQ(Add);
      CASE_ASSIGN_EQ(Sub);
      CASE_ASSIGN_EQ(Mul);
      CASE_ASSIGN_EQ(Div);
      CASE_ASSIGN_EQ(Mod);
#undef CASE_ASSIGN_EQ
    case Language::Operator::Index: return {PtrCallFix(EmitLVal(ctx)[0])};
    default: UNREACHABLE(*this);
  }
}

std::vector<IR::Val> AST::Binop::EmitLVal(Context *ctx) {
  switch (op) {
    case Language::Operator::As: NOT_YET();
    case Language::Operator::Index:
      if (lhs->type->is<type::Array>()) {
        return {IR::Index(lhs->EmitLVal(ctx)[0], rhs->EmitIR(ctx)[0])};
      }
      [[fallthrough]];
    default: UNREACHABLE("Operator is ", static_cast<int>(op));
  }
}

Binop *Binop::Clone() const {
  auto *result = new Binop;
  result->span = span;
  result->lhs  = base::wrap_unique(lhs->Clone());
  result->rhs  = base::wrap_unique(rhs->Clone());
  result->op   = op;
  return result;
}


}  // namespace AST
