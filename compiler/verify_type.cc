#include "absl/algorithm/container.h"

#include <iostream>
#include <optional>
#include <string_view>

#include "ast/ast.h"
#include "ast/overload_set.h"
#include "compiler/compiler.h"
#include "compiler/dispatch/parameters_and_arguments.h"
#include "compiler/dispatch/scope_table.h"
#include "compiler/extract_jumps.h"
#include "compiler/library_module.h"
#include "diagnostic/consumer/streaming.h"
#include "diagnostic/errors.h"
#include "error/inference_failure_reason.h"
#include "frontend/operators.h"
#include "interpretter/evaluate.h"
#include "ir/compiled_fn.h"
#include "type/cast.h"
#include "type/generic_struct.h"
#include "type/jump.h"
#include "type/parameter_pack.h"
#include "type/type.h"
#include "type/typed_value.h"
#include "type/util.h"

namespace ir {

// TODO Duplicated in emit_value.h
static type::Type const *BuiltinType(core::Builtin b) {
  switch (b) {
#define ICARUS_CORE_BUILTIN_X(enumerator, str, t)                              \
  case core::Builtin::enumerator:                                              \
    return t;
#include "core/builtin.xmacro.h"
#undef ICARUS_CORE_BUILTIN_X
  }
  UNREACHABLE();
}
}  // namespace ir

namespace compiler {
namespace {

void AddAdl(ast::OverloadSet *overload_set, std::string_view id,
            type::Type const *t) {
  absl::flat_hash_set<module::BasicModule const *> modules;
  // TODO t->ExtractDefiningModules(&modules);

  for (auto const *mod : modules) {
    auto decls = mod->declarations(id);
      diagnostic::StreamingConsumer consumer(stderr);

    for (auto *d : decls) {
      // TODO Wow this is a terrible way to access the type.
      ASSIGN_OR(continue, auto &t,
                Compiler(const_cast<module::BasicModule *>(mod), consumer)
                    .type_of(d));
      // TODO handle this case. I think it's safe to just discard it.
      for (auto const *expr : overload_set->members()) {
        if (d == expr) { return; }
      }

      // TODO const
      overload_set->insert(d);
    }
  }
}

type::QualType VerifyUnaryOverload(Compiler *c, char const *symbol,
                                 ast::Expression const *node,
                                 type::QualType operand_result) {
  ast::OverloadSet os(node->scope_, symbol);
  AddAdl(&os, symbol, operand_result.type());
  ASSIGN_OR(
      return type::QualType::Error(), auto table,
             FnCallDispatchTable::Verify(
                 c, os, core::FnArgs<type::QualType>({operand_result}, {})));
  auto result = type::QualType::NonConstant(table.result_type());
  c->data_.set_dispatch_table(node, std::move(table));
  return c->set_result(node, result);
}

type::QualType VerifyBinaryOverload(Compiler *c, char const *symbol,
                                  ast::Expression const *node,
                                  type::QualType lhs_result,
                                  type::QualType rhs_result) {
  ast::OverloadSet os(node->scope_, symbol);
  AddAdl(&os, symbol, lhs_result.type());
  AddAdl(&os, symbol, rhs_result.type());
  ASSIGN_OR(
      return type::QualType::Error(), auto table,
             FnCallDispatchTable::Verify(
                 c, os,
                 core::FnArgs<type::QualType>({lhs_result, rhs_result}, {})));
  auto result = type::QualType::NonConstant(table.result_type());
  c->data_.set_dispatch_table(node, std::move(table));
  return c->set_result(node, result);
}
// NOTE: the order of these enumerators is meaningful and relied upon! They are
// ordered from strongest relation to weakest.
enum class Cmp { Order, Equality, None };

Cmp Comparator(type::Type const *t);

template <typename TypeContainer>
Cmp MinCmp(TypeContainer const &c) {
  using cmp_t = std::underlying_type_t<Cmp>;
  return static_cast<Cmp>(absl::c_accumulate(
      c, static_cast<cmp_t>(Cmp::Equality), [](cmp_t val, type::Type const *t) {
        return std::min(val, static_cast<cmp_t>(Comparator(t)));
      }));
}

Cmp Comparator(type::Type const *t) {
  using cmp_t = std::underlying_type_t<Cmp>;
  if (auto *v = t->if_as<type::Variant>()) { return MinCmp(v->variants_); }
  if (auto *tup = t->if_as<type::Tuple>()) { return MinCmp(tup->entries_); }
  if (auto *a = t->if_as<type::Array>()) {
    return static_cast<Cmp>(
        std::min(static_cast<cmp_t>(Comparator(a->data_type)),
                 static_cast<cmp_t>(Cmp::Equality)));
  } else if (auto *p = t->if_as<type::Primitive>()) {
    return type::IsNumeric(p) ? Cmp::Order : Cmp::Equality;
  } else if (t->is<type::Flags>() or t->is<type::BufferPointer>()) {
    return Cmp::Order;
  } else if (t->is<type::Enum>() or t->is<type::Pointer>()) {
    return Cmp::Equality;
  } else {
    return Cmp::None;
  }
}

bool VerifyAssignment(Compiler *c, frontend::SourceRange const &span,
                      type::Type const *to, type::Type const *from) {
  if (to == from and to->is<type::GenericStruct>()) { return true; }

  // TODO this feels like the semantics are iffy. It works fine if we assign
  // to/from the same type, but we really care if you can assign to a type
  // rather than copy from another, I think.
  if (not from->IsMovable()) {
    c->error_log()->NotMovable(span, from->to_string());
    return false;
  }

  if (to == from) { return true; }
  auto *to_tup   = to->if_as<type::Tuple>();
  auto *from_tup = from->if_as<type::Tuple>();
  if (to_tup and from_tup) {
    if (to_tup->size() != from_tup->size()) {
      c->error_log()->MismatchedAssignmentSize(span, to_tup->size(),
                                               from_tup->size());
      return false;
    }

    bool result = true;
    for (size_t i = 0; i < to_tup->size(); ++i) {
      result &= VerifyAssignment(c, span, to_tup->entries_.at(i),
                                 from_tup->entries_.at(i));
    }
    return result;
  }

  if (auto *to_var = to->if_as<type::Variant>()) {
    if (auto *from_var = from->if_as<type::Variant>()) {
      for (auto fvar : from_var->variants_) {
        if (not to_var->contains(fvar)) {
          NOT_YET("log an error", from, to);
          return false;
        }
      }
      return true;
    } else {
      if (not to_var->contains(from)) {
        NOT_YET("log an error", from, to);
        return false;
      }

      return true;
    }
  }

  if (auto *to_ptr = to->if_as<type::Pointer>()) {
    if (from == type::NullPtr) { return true; }
    NOT_YET("log an error", from, to);
    return false;
  }

  NOT_YET("log an error: no cast from ", from->to_string(), " to ",
          to->to_string());
}

type::Type const *DereferenceAll(type::Type const *t) {
  while (auto *p = t->if_as<type::Pointer>()) { t = p->pointee; }
  return t;
}

}  // namespace

static std::optional<std::vector<type::QualType>> VerifyWithoutSetting(
    Compiler *visitor, base::PtrSpan<ast::Expression const> exprs) {
  std::vector<type::QualType> results;
  results.reserve(exprs.size());
  for (auto const &expr : exprs) {
    auto r = visitor->Visit(expr, VerifyTypeTag{});
    if (expr->needs_expansion()) {
      auto &entries = r.type()->as<type::Tuple>().entries_;
      for (auto *t : entries) { results.emplace_back(t, r.constant()); }
    } else {
      results.push_back(r);
    }
  }
  if (absl::c_any_of(results,
                     [](type::QualType const &r) { return not r.ok(); })) {
    return std::nullopt;
  }
  return results;
}

static type::QualType VerifySpecialFunctions(Compiler *visitor,
                                           ast::Declaration const *decl,
                                           type::Type const *decl_type) {
  bool error = false;
  if (decl->id() == "copy") {
    if (auto *f = decl_type->if_as<type::Function>()) {
      if (not f->output.empty()) {
        error = true;
        NOT_YET("output must be empty");
      }

      if (f->input.size() != 2 or f->input.at(0) != f->input.at(1) or
          not f->input.at(0)->is<type::Pointer>() or
          not f->input.at(0)->as<type::Pointer>().pointee->is<type::Struct>()) {
        error = true;
        NOT_YET("incorrect input type");
      } else {
        // TODO should you check that they're exported consistently in some way?
        // Note that you don't export the struct but rather declarations bound
        // to it so it's not totally clear how you would do that.
        auto const &s =
            f->input.at(0)->as<type::Pointer>().pointee->as<type::Struct>();

        if (decl->scope_ != s.scope_) {
          error = true;
          NOT_YET(
              "(copy) must be defined in the same scope as the corresponding "
              "type");
        }

        if (s.contains_hashtag(ast::Hashtag::Builtin::Uncopyable)) {
          NOT_YET("defined (copy) on a non-copyable type");
        }
      }
    } else {
      error = true;
      NOT_YET("log an error. (copy) must be a function.");
    }
  } else if (decl->id() == "move") {
    if (auto *f = decl_type->if_as<type::Function>()) {
      if (not f->output.empty()) {
        error = true;
        NOT_YET("output must be empty");
      }

      if (f->input.size() != 2 or f->input.at(0) != f->input.at(1) or
          not f->input.at(0)->is<type::Pointer>() or
          not f->input.at(0)->as<type::Pointer>().pointee->is<type::Struct>()) {
        error = true;
        NOT_YET("incorrect input type");
      } else {
        // TODO should you check that they're exported consistently in some way?
        // Note that you don't export the struct but rather declarations bound
        // to it so it's not totally clear how you would do that.
        auto const &s =
            f->input.at(0)->as<type::Pointer>().pointee->as<type::Struct>();

        if (decl->scope_ != s.scope_) {
          error = true;
          NOT_YET(
              "(move) must be defined in the same scope as the corresponding "
              "type");
        }

        if (s.contains_hashtag(ast::Hashtag::Builtin::Immovable)) {
          error = true;
          NOT_YET("defined (move) for an immovable type");
        }
      }
    } else {
      error = true;
      NOT_YET("log an error. (move) must be a function.");
    }
  }
  if (error) { visitor->set_result(decl, type::QualType::Error()); }

  return visitor->set_result(
      decl,
      type::QualType(decl_type, decl->flags() & ast::Declaration::f_IsConst));
}

// TODO what about shadowing of symbols across module boundaries imported with
// -- ::= ?
// Or when you import two modules verifying that symbols don't conflict.
bool Shadow(Compiler *compiler, type::Typed<ast::Declaration const *> decl1,
            type::Typed<ast::Declaration const *> decl2) {
  // TODO Don't worry about generic shadowing? It'll be checked later?
  if (decl1.type() == type::Generic or decl2.type() == type::Generic) {
    return false;
  }

  return core::AmbiguouslyCallable(
      ExtractParams(compiler, *decl1).Transform([](auto const &typed_decl) {
        return typed_decl.type();
      }),
      ExtractParams(compiler, *decl2).Transform([](auto const &typed_decl) {
        return typed_decl.type();
      }),
      [](type::Type const *lhs, type::Type const *rhs) {
        return type::Meet(lhs, rhs) != nullptr;
      });
}

enum DeclKind { INFER = 1, CUSTOM_INIT = 2, UNINITIALIZED = 4 };

static InferenceFailureReason Inferrable(type::Type const *t) {
  if (t == type::NullPtr) { return InferenceFailureReason::NullPtr; }
  if (t == type::EmptyArray) { return InferenceFailureReason::EmptyArray; }
  if (auto *a = t->if_as<type::Array>()) { return Inferrable(a->data_type); }
  if (auto *p = t->if_as<type::Pointer>()) { return Inferrable(p->pointee); }
  if (auto *v = t->if_as<type::Variant>()) {
    // TODO only returning the first failure here and not even givving a good
    // explanation of precisely what the problem is. Fix here and below.
    for (auto const *var : v->variants_) {
      auto reason = Inferrable(var);
      if (reason != InferenceFailureReason::Inferrable) { return reason; }
    }
  } else if (auto *tup = t->if_as<type::Tuple>()) {
    for (auto const *entry : tup->entries_) {
      auto reason = Inferrable(entry);
      if (reason != InferenceFailureReason::Inferrable) { return reason; }
    }
  } else if (auto *f = t->if_as<type::Function>()) {
    for (auto const *t : f->input) {
      auto reason = Inferrable(t);
      if (reason != InferenceFailureReason::Inferrable) { return reason; }
    }
    for (auto const *t : f->output) {
      auto reason = Inferrable(t);
      if (reason != InferenceFailureReason::Inferrable) { return reason; }
    }
  }
  // TODO higher order types?
  return InferenceFailureReason::Inferrable;
}

// TODO there's not that much shared between the inferred and uninferred cases,
// so probably break them out.
type::QualType VerifyBody(Compiler *c, ast::FunctionLiteral const *node) {
  for (auto const *stmt : node->stmts()) {
    c->Visit(stmt, VerifyTypeTag{});
  }
  // TODO propogate cyclic dependencies.

  ExtractJumps extractor;
  for (auto const *stmt : node->stmts()) { extractor.Visit(stmt); }

  // TODO we can have yields and returns, or yields and jumps, but not jumps and
  // returns. Check this.
  absl::flat_hash_set<type::Type const *> types;
  absl::flat_hash_map<ast::ReturnStmt const *, type::Type const *>
      saved_ret_types;
  for (auto const *n : extractor.jumps(ExtractJumps::Kind::Return)) {
    if (auto const *ret_node = n->if_as<ast::ReturnStmt>()) {
      std::vector<type::Type const *> ret_types;
      for (auto const *expr : ret_node->exprs()) {
        ret_types.push_back(c->type_of(expr));
      }
      auto *t = Tup(std::move(ret_types));
      types.emplace(t);
      saved_ret_types.emplace(ret_node, t);
    } else {
      UNREACHABLE();  // TODO
    }
  }

  std::vector<type::Type const *> input_type_vec;
  input_type_vec.reserve(node->params().size());
  for (auto &param : node->params()) {
    input_type_vec.push_back(
        ASSERT_NOT_NULL(c->type_of(param.value.get())));
  }

  if (not node->outputs()) {
    std::vector<type::Type const *> output_type_vec(
        std::make_move_iterator(types.begin()),
        std::make_move_iterator(types.end()));

    if (types.size() > 1) { NOT_YET("log an error"); }
    auto f = type::Func(std::move(input_type_vec), std::move(output_type_vec));
    return c->set_result(node, type::QualType::Constant(f));

  } else {
    auto *node_type  = c->type_of(node);
    auto const &outs = ASSERT_NOT_NULL(node_type)->as<type::Function>().output;
    switch (outs.size()) {
      case 0: {
        bool err = false;
        for (auto *n : extractor.jumps(ExtractJumps::Kind::Return)) {
          if (auto *ret_node = n->if_as<ast::ReturnStmt>()) {
            if (not ret_node->exprs().empty()) {
              c->error_log()->NoReturnTypes(ret_node);
              err = true;
            }
          } else {
            UNREACHABLE();  // TODO
          }
        }
        return err ? type::QualType::Error() : type::QualType::Constant(node_type);
      } break;
      case 1: {
        bool err = false;
        for (auto *n : extractor.jumps(ExtractJumps::Kind::Return)) {
          if (auto *ret_node = n->if_as<ast::ReturnStmt>()) {
            auto *t = ASSERT_NOT_NULL(saved_ret_types.at(ret_node));
            if (t == outs[0]) { continue; }
            c->error_log()->ReturnTypeMismatch(
                outs[0]->to_string(), t->to_string(), ret_node->span);
            err = true;
          } else {
            UNREACHABLE();  // TODO
          }
        }
        return err ? type::QualType::Error() : type::QualType::Constant(node_type);
      } break;
      default: {
        for (auto *n : extractor.jumps(ExtractJumps::Kind::Return)) {
          if (auto *ret_node = n->if_as<ast::ReturnStmt>()) {
            auto *expr_type = ASSERT_NOT_NULL(saved_ret_types.at(ret_node));
            if (expr_type->is<type::Tuple>()) {
              auto const &tup_entries = expr_type->as<type::Tuple>().entries_;
              if (tup_entries.size() != outs.size()) {
                c->error_log()->ReturningWrongNumber(
                    ret_node->span,
                    (expr_type->is<type::Tuple>()
                         ? expr_type->as<type::Tuple>().size()
                         : 1),
                    outs.size());
                return type::QualType::Error();
              } else {
                bool err = false;
                for (size_t i = 0; i < tup_entries.size(); ++i) {
                  if (tup_entries.at(i) != outs.at(i)) {
                    // TODO if this is a commalist we can point to it more
                    // carefully but if we're just passing on multiple return
                    // values it's harder.
                    //
                    // TODO point the span to the correct entry which may be
                    // hard if it's splatted.
                    c->error_log()->IndexedReturnTypeMismatch(
                        outs.at(i)->to_string(), tup_entries.at(i)->to_string(),
                        ret_node->span, i);
                    err = true;
                  }
                }
                if (err) { return type::QualType::Error(); }
              }
            } else {
              c->error_log()->ReturningWrongNumber(
                  ret_node->span,
                  (expr_type->is<type::Tuple>()
                       ? expr_type->as<type::Tuple>().size()
                       : 1),
                  outs.size());
              return type::QualType::Error();
            }
          } else {
            UNREACHABLE();  // TODO
          }
        }
        return type::QualType::Constant(node_type);
      } break;
    }
  }
}

void VerifyBody(Compiler *c, ast::Jump const *node) {
  for (auto const *stmt : node->stmts()) {
    c->Visit(stmt, VerifyTypeTag{});
  }
}

type::QualType Compiler::VerifyConcreteFnLit(ast::FunctionLiteral const *node) {
  std::vector<type::Type const *> input_type_vec;
  input_type_vec.reserve(node->params().size());
  for (auto &d : node->params()) {
    ASSIGN_OR(return _, auto result, Visit(d.value.get(), VerifyTypeTag{}));
    input_type_vec.push_back(result.type());
  }

  std::vector<type::Type const *> output_type_vec;
  bool error   = false;
  auto outputs = node->outputs();
  if (outputs) {
    output_type_vec.reserve(outputs->size());
    for (auto *output : *outputs) {
      auto result = Visit(output, VerifyTypeTag{});
      output_type_vec.push_back(result.type());
      if (result.type() != nullptr and not result.constant()) {
        // TODO this feels wrong because output could be a decl. And that decl
        // being a const decl isn't what I care about.
        NOT_YET("log an error");
        error = true;
      }
    }
  }

  if (error or
      absl::c_any_of(input_type_vec,
                     [](type::Type const *t) { return t == nullptr; }) or
      absl::c_any_of(output_type_vec,
                     [](type::Type const *t) { return t == nullptr; })) {
    return type::QualType::Error();
  }

  // TODO need a better way to say if there was an error recorded in a
  // particular section of compilation. Right now we just have the grad total
  // count.
  if (num_errors() > 0) {
    error_log()->Dump();
    return type::QualType::Error();
  }

  if (outputs) {
    for (size_t i = 0; i < output_type_vec.size(); ++i) {
      if (auto *decl = (*outputs)[i]->if_as<ast::Declaration>()) {
        output_type_vec.at(i) = type_of(decl);
      } else {
        ASSERT(output_type_vec.at(i) == type::Type_);
        output_type_vec.at(i) = interpretter::EvaluateAs<type::Type const *>(
            MakeThunk((*outputs)[i], type::Type_));
      }
    }

    return set_result(
        node, type::QualType::Constant(type::Func(std::move(input_type_vec),
                                                std::move(output_type_vec))));
  } else {
    return VerifyBody(this, node);
  }
}

static std::optional<
    std::vector<std::pair<ast::Expression const *, type::QualType>>>
VerifySpan(Compiler *v, base::PtrSpan<ast::Expression const> exprs) {
  // TODO expansion
  std::vector<std::pair<ast::Expression const *, type::QualType>> results;
  bool err = false;
  for (auto *expr : exprs) {
    results.emplace_back(expr, v->Visit(expr, VerifyTypeTag{}));
    err |= not results.back().second.ok();
  }
  if (err) { return std::nullopt; }
  return results;
}

enum class Constness { Error, Const, NonConst };
static Constness VerifyAndGetConstness(
    Compiler *v, base::PtrSpan<ast::Expression const> exprs) {
  bool err      = false;
  bool is_const = true;
  for (auto *expr : exprs) {
    auto r = v->Visit(expr, VerifyTypeTag{});
    err |= not r.ok();
    if (not err) { is_const &= r.constant(); }
  }
  if (err) { return Constness::Error; }
  return is_const ? Constness::Const : Constness::NonConst;
}

static type::QualType AccessTypeMember(Compiler *c, ast::Access const *node,
                                     type::QualType operand_result) {
  if (not operand_result.constant()) {
    c->error_log()->NonConstantTypeMemberAccess(node->span);
    return type::QualType::Error();
  }
  // TODO We may not be allowed to evaluate node:
  //    f ::= (T: type) => T.key
  // We need to know that T is const
  auto *evaled_type = interpretter::EvaluateAs<type::Type const *>(
      c->MakeThunk(node->operand(), operand_result.type()));

  // For enums and flags, regardless of whether we can get the value, it's
  // clear that node is supposed to be a member so we should emit an error but
  // carry on assuming that node is an element of that enum type.
  if (auto *e = evaled_type->if_as<type::Enum>()) {
    if (not e->Get(node->member_name()).has_value()) {
      c->error_log()->MissingMember(node->span, node->member_name(),
                                    evaled_type->to_string());
    }
    return c->set_result(node, type::QualType::Constant(evaled_type));
  } else if (auto *f = evaled_type->if_as<type::Flags>()) {
    if (not f->Get(node->member_name()).has_value()) {
      c->error_log()->MissingMember(node->span, node->member_name(),
                                    evaled_type->to_string());
    }
    return c->set_result(node, type::QualType::Constant(evaled_type));
  } else {
    // TODO what about structs? Can structs have constant members we're
    // allowed to access?
    c->error_log()->TypeHasNoMembers(node->span);
    return type::QualType::Error();
  }
}

static type::QualType AccessStructMember(Compiler *c, ast::Access const *node,
                                       type::QualType operand_result) {
  auto const &s      = operand_result.type()->as<type::Struct>();
  auto const *member = s.field(node->member_name());
  if (member == nullptr) {
    c->error_log()->MissingMember(node->span, node->member_name(),
                                  s.to_string());
    return type::QualType::Error();
  }
  if (c->module() != s.defining_module() and
      not member->contains_hashtag(ast::Hashtag::Builtin::Export)) {
    c->error_log()->NonExportedMember(node->span, node->member_name(),
                                      s.to_string());
  }

  return c->set_result(node,
                       type::QualType(member->type, operand_result.constant()));
}

static type::QualType AccessModuleMember(Compiler *c, ast::Access const *node,
                                       type::QualType operand_result) {
  DEBUG_LOG("AccessModuleMember")(node->DebugString());
  if (not operand_result.constant()) {
    c->error_log()->NonConstantModuleMemberAccess(node->span);
    return type::QualType::Error();
  }

  DEBUG_LOG("AccessModuleMember")(node->DebugString());
  // TODO this is a common pattern for dealing with imported modules. Extract
  // it.
  auto *mod = interpretter::EvaluateAs<CompiledModule const *>(
      c->MakeThunk(node->operand(), type::Module));
  auto decls = mod->declarations(node->member_name());
  switch (decls.size()) {
    case 0: {
      NOT_YET("Log an error, no such symbol in module.");
    } break;
    case 1: {
      type::Type const *t = mod->type_of(decls[0]);
      if (t == nullptr) {
        c->error_log()->NoExportedSymbol(node->span);
        return type::QualType::Error();
      }
      DEBUG_LOG("AccessModuleMember")
      ("Setting type of ", node, " to ", type::QualType::Constant(t), " on ", c);
      return c->set_result(node, type::QualType::Constant(t));

    } break;
    default: {
      NOT_YET("Ambiguous, multiple possible symbols exported by module.");
    } break;
  }
  UNREACHABLE();
}

type::QualType Compiler::Visit(ast::Access const *node, VerifyTypeTag) {
  ASSIGN_OR(return type::QualType::Error(), auto operand_result,
                   Visit(node->operand(), VerifyTypeTag{}));

  auto base_type = DereferenceAll(operand_result.type());
  if (base_type == type::Type_) {
    return AccessTypeMember(this, node, operand_result);
  } else if (auto *s = base_type->if_as<type::Struct>()) {
    return AccessStructMember(this, node, operand_result);
  } else if (base_type == type::Module) {
    return AccessModuleMember(this, node, operand_result);
  } else {
    error_log()->MissingMember(node->span, node->member_name(),
                               base_type->to_string());
    return type::QualType::Error();
  }
}

type::QualType Compiler::Visit(ast::ArrayLiteral const *node, VerifyTypeTag) {
  if (node->empty()) {
    return set_result(node, type::QualType::Constant(type::EmptyArray));
  }

  ASSIGN_OR(return type::QualType::Error(), auto expr_results,
                   VerifyWithoutSetting(this, node->elems()));
  auto *t                       = expr_results.front().type();
  type::Type const *result_type = type::Arr(expr_results.size(), t);
  bool constant                 = true;
  for (auto expr_result : expr_results) {
    constant &= expr_result.constant();
    if (expr_result.type() != t) {
      error_log()->InconsistentArrayType(node->span);
      return type::QualType::Error();
    }
  }
  return set_result(node, type::QualType(result_type, constant));
}

type::QualType Compiler::Visit(ast::ArrayType const *node, VerifyTypeTag) {
  std::vector<type::QualType> length_results;
  length_results.reserve(node->lengths().size());
  bool is_const = true;
  for (auto const &len : node->lengths()) {
    auto result = Visit(len, VerifyTypeTag{});
    is_const &= result.constant();
    length_results.push_back(result);
    if (result.type() != type::Int64) {
      error_log()->ArrayIndexType(node->span);
    }
  }

  auto data_type_result = Visit(node->data_type(), VerifyTypeTag{});
  if (data_type_result.type() != type::Type_) {
    error_log()->ArrayDataTypeNotAType(node->data_type()->span);
  }

  return set_result(
      node,
      type::QualType(type::Type_, data_type_result.constant() and is_const));
}

static bool IsTypeOrTupleOfTypes(type::Type const *t) {
  return t == type::Type_ or t->is<type::Tuple>();
}

type::QualType Compiler::Visit(ast::Binop const *node, VerifyTypeTag) {
  auto lhs_result = Visit(node->lhs(), VerifyTypeTag{});
  auto rhs_result = Visit(node->rhs(), VerifyTypeTag{});
  if (not lhs_result.ok() or not rhs_result.ok()) {
    return type::QualType::Error();
  }

  using frontend::Operator;
  switch (node->op()) {
    case Operator::Assign: {
      // TODO if lhs is reserved?
      if (not VerifyAssignment(this, node->span, lhs_result.type(),
                               rhs_result.type())) {
        return type::QualType::Error();
      }
      return type::QualType::NonConstant(type::Void());
    } break;
    case Operator::XorEq:
      if (lhs_result.type() == rhs_result.type() and
          (lhs_result.type() == type::Bool or
           lhs_result.type()->is<type::Flags>())) {
        return set_result(node, lhs_result);
      } else {
        error_log()->XorEqNeedsBoolOrFlags(node->span);
        return type::QualType::Error();
      }
    case Operator::AndEq:
      if (lhs_result.type() == rhs_result.type() and
          (lhs_result.type() == type::Bool or
           lhs_result.type()->is<type::Flags>())) {
        return set_result(node, lhs_result);
      } else {
        error_log()->AndEqNeedsBoolOrFlags(node->span);
        return type::QualType::Error();
      }
    case Operator::OrEq:
      if (lhs_result.type() == rhs_result.type() and
          (lhs_result.type() == type::Bool or
           lhs_result.type()->is<type::Flags>())) {
        return set_result(node, lhs_result);
      } else {
        error_log()->OrEqNeedsBoolOrFlags(node->span);
        return type::QualType::Error();
      }

#define CASE(symbol, return_type)                                              \
  do {                                                                         \
    bool is_const = lhs_result.constant() and rhs_result.constant();           \
    if (type::IsNumeric(lhs_result.type()) and                                 \
        type::IsNumeric(rhs_result.type())) {                                  \
      if (lhs_result.type() == rhs_result.type()) {                            \
        return set_result(node, type::QualType((return_type), is_const));      \
      } else {                                                                 \
        diag_consumer_.Consume(                                                \
            diagnostic::ArithmeticBinaryOperatorTypeMismatch{                  \
                .lhs_type = lhs_result.type(),                                 \
                .rhs_type = rhs_result.type(),                                 \
                .range    = node->span}                                        \
                .ToMessage());                                                 \
        return type::QualType::Error();                                        \
      }                                                                        \
    } else {                                                                   \
      return VerifyBinaryOverload(this, symbol, node, lhs_result, rhs_result); \
    }                                                                          \
  } while (false)

    case Operator::Sub: {
      CASE("-", lhs_result.type());
    } break;
    case Operator::Mul: {
      CASE("*", lhs_result.type());
    } break;
    case Operator::Div: {
      CASE("/", lhs_result.type());
    } break;
    case Operator::Mod: {
      CASE("%", lhs_result.type());
    } break;
    case Operator::SubEq: {
      CASE("-=", type::Void());
    } break;
    case Operator::MulEq: {
      CASE("*=", type::Void());
    } break;
    case Operator::DivEq: {
      CASE("/=", type::Void());
    } break;
    case Operator::ModEq: {
      CASE("%=", type::Void());
    } break;
#undef CASE
    case Operator::Add: {
      bool is_const = lhs_result.constant() and rhs_result.constant();
      if (type::IsNumeric(lhs_result.type()) and
          type::IsNumeric(rhs_result.type())) {
        if (lhs_result.type() == rhs_result.type()) {
          return set_result(node, type::QualType(lhs_result.type(), is_const));
        } else {
          diag_consumer_.Consume(
              diagnostic::ArithmeticBinaryOperatorTypeMismatch{
                  .lhs_type = lhs_result.type(),
                  .rhs_type = rhs_result.type(),
                  .range    = node->span}
                  .ToMessage());
          return type::QualType::Error();
        }
      } else {
        VerifyBinaryOverload(this, "+", node, lhs_result, rhs_result);
      }
    } break;
    case Operator::AddEq: {
      bool is_const = lhs_result.constant() and rhs_result.constant();
      if (type::IsNumeric(lhs_result.type()) and
          type::IsNumeric(rhs_result.type())) {
        if (lhs_result.type() == rhs_result.type()) {
          return set_result(node, type::QualType(type::Void(), is_const));
        } else {
          diag_consumer_.Consume(
              diagnostic::ArithmeticBinaryOperatorTypeMismatch{
                  .lhs_type = lhs_result.type(),
                  .rhs_type = rhs_result.type(),
                  .range    = node->span}
                  .ToMessage());
          return type::QualType::Error();
        }
      } else {
        return VerifyBinaryOverload(this, "+=", node, lhs_result, rhs_result);
      }
    } break;
    case Operator::Arrow: {
      type::Type const *t = type::Type_;
      if (not IsTypeOrTupleOfTypes(lhs_result.type())) {
        t = nullptr;
        error_log()->NonTypeFunctionInput(node->span);
      }

      if (not IsTypeOrTupleOfTypes(rhs_result.type())) {
        t = nullptr;
        error_log()->NonTypeFunctionOutput(node->span);
      }

      if (t == nullptr) { return type::QualType::Error(); }

      return set_result(
          node, type::QualType(type::Type_,
                             lhs_result.constant() and rhs_result.constant()));
    }
    default: UNREACHABLE();
  }
  UNREACHABLE(static_cast<int>(node->op()));
}

type::QualType Compiler::Visit(ast::BlockLiteral const *node, VerifyTypeTag) {
  // TODO consider not verifying the types of the bodies. They almost certainly
  // contain circular references in the jump statements, and if the functions
  // require verifying the body upfront, things can maybe go wrong?
  for (auto *b : node->before()) { Visit(b, VerifyTypeTag{}); }
  for (auto *a : node->after()) { Visit(a, VerifyTypeTag{}); }

  return set_result(node, type::QualType::Constant(type::Block));
}

type::QualType Compiler::Visit(ast::BlockNode const *node, VerifyTypeTag) {
  for (auto *arg : node->args()) { Visit(arg, VerifyTypeTag{}); }
  for (auto *stmt : node->stmts()) { Visit(stmt, VerifyTypeTag{}); }
  return set_result(node, type::QualType::Constant(type::Block));
}

type::QualType Compiler::Visit(ast::BuiltinFn const *node, VerifyTypeTag) {
  return set_result(node,
                    type::QualType::Constant(ir::BuiltinType(node->value())));
}

static ast::OverloadSet FindOverloads(
    ast::Scope const *scope, std::string_view token,
    core::FnArgs<type::QualType, std::string_view> args) {
  ast::OverloadSet os(scope, token);
  for (type::QualType result : args) { AddAdl(&os, token, result.type()); };
  DEBUG_LOG("FindOverloads")
  ("Found ", os.members().size(), " overloads for '", token, "'");
  return os;
}

std::optional<ast::OverloadSet> MakeOverloadSet(
    Compiler *c, ast::Expression const *expr,
    core::FnArgs<type::QualType, std::string_view> const &args) {
  if (auto *id = expr->if_as<ast::Identifier>()) {
    return FindOverloads(expr->scope_, id->token(), args);
  } else if (auto *acc = expr->if_as<ast::Access>()) {
    ASSIGN_OR(return std::nullopt,  //
                     auto result, c->Visit(acc->operand(), VerifyTypeTag{}));
    if (result.type() == type::Module) {
      // TODO this is a common pattern for dealing with imported modules.
      // Extract it.
      auto *mod = interpretter::EvaluateAs<CompiledModule const *>(
          c->MakeThunk(acc->operand(), type::Module));
      return FindOverloads(mod->scope(), acc->member_name(), args);
    }
  }

  ast::OverloadSet os;
  os.insert(expr);
  // TODO ADL for node?
  return os;
}

template <typename EPtr, typename StrType>
static type::QualType VerifyCall(Compiler *c, ast::BuiltinFn const *b,
                               core::FnArgs<EPtr, StrType> const &args,
                               core::FnArgs<type::QualType> const &arg_results) {
  switch (b->value()) {
    case core::Builtin::Foreign: {
      bool err = false;
      if (not arg_results.named().empty()) {
        c->error_log()->BuiltinError(b->span,
                                           "Built-in function `foreign` cannot "
                                           "be called with named arguments.");
        err = true;
      }

      size_t size = arg_results.size();
      if (size != 2u) {
        c->error_log()->BuiltinError(
            b->span, absl::StrCat("Built-in function `foreign` takes exactly "
                                  "two arguments (You provided ",
                                  size, ")."));
        err = true;
      }

      if (not err) {
        if (arg_results.at(0).type() != type::ByteView) {
          c->error_log()->BuiltinError(
              b->span,
              absl::StrCat("First argument to `foreign` must be a byte-view "
                           "(You provided a(n) ",
                           arg_results.at(0).type()->to_string(), ")."));
        }
        if (not arg_results.at(0).constant()) {
          c->error_log()->BuiltinError(
              b->span, "First argument to `foreign` must be a constant.");
        }
        if (arg_results.at(1).type() != type::Type_) {
          c->error_log()->BuiltinError(
              b->span,
              "Second argument to `foreign` must be a type (You provided "
              "a(n) " +
                  arg_results.at(0).type()->to_string() + ").");
        }
        if (not arg_results.at(1).constant()) {
          c->error_log()->BuiltinError(
              b->span, "Second argument to `foreign` must be a constant.");
        }
      }
      return type::QualType::Constant(
          interpretter::EvaluateAs<type::Type const *>(
              c->MakeThunk(args.at(1), type::Type_)));
    } break;
    case core::Builtin::Opaque:
      if (not arg_results.empty()) {
        c->error_log()->BuiltinError(
            b->span, "Built-in function `opaque` takes no arguments.");
      }
      return type::QualType::Constant(ir::BuiltinType(core::Builtin::Opaque)
                                        ->as<type::Function>()
                                        .output[0]);

    case core::Builtin::Bytes: {
      size_t size = arg_results.size();
      if (not arg_results.named().empty()) {
        c->error_log()->BuiltinError(
            b->span,
            "Built-in function `bytes` cannot be "
            "called with named arguments.");
      } else if (size != 1u) {
        c->error_log()->BuiltinError(
            b->span,
            "Built-in function `bytes` takes "
            "exactly one argument (You provided " +
                std::to_string(size) + ").");
      } else if (arg_results.at(0).type() != type::Type_) {
        c->error_log()->BuiltinError(
            b->span,
            "Built-in function `bytes` must take a single argument of type "
            "`type` (You provided a(n) " +
                arg_results.at(0).type()->to_string() + ").");
      }
      return type::QualType::Constant(ir::BuiltinType(core::Builtin::Bytes)
                                        ->as<type::Function>()
                                        .output[0]);
    }
    case core::Builtin::Alignment: {
      size_t size = arg_results.size();
      if (not arg_results.named().empty()) {
        c->error_log()->BuiltinError(
            b->span,
            "Built-in function `alignment` cannot "
            "be called with named arguments.");
      }
      if (size != 1u) {
        c->error_log()->BuiltinError(
            b->span,
            "Built-in function `alignment` takes "
            "exactly one argument (You provided " +
                std::to_string(size) + ").");

      } else if (arg_results.at(0).type() != type::Type_) {
        c->error_log()->BuiltinError(
            b->span,
            "Built-in function `alignment` must take a single argument of "
            "type `type` (you provided a(n) " +
                arg_results.at(0).type()->to_string() + ")");
      }
      return type::QualType::Constant(ir::BuiltinType(core::Builtin::Alignment)
                                        ->as<type::Function>()
                                        .output[0]);
    }
#if defined(ICARUS_DEBUG)
    case core::Builtin::DebugIr:
      // This is for debugging the compiler only, so there's no need to write
      // decent errors here.
      ASSERT(arg_results, matcher::IsEmpty());
      return type::QualType::Constant(type::Void());
#endif  // defined(ICARUS_DEBUG)
  }
  UNREACHABLE();
}

template <typename EPtr, typename StrType>
static std::pair<core::FnArgs<type::QualType, StrType>, bool> VerifyFnArgs(
    Compiler *visitor, core::FnArgs<EPtr, StrType> const &args) {
  bool err         = false;
  auto arg_results = args.Transform([&](EPtr const &expr) {
    auto expr_result = visitor->Visit(expr, VerifyTypeTag{});
    err |= not expr_result.ok();
    return expr_result;
  });

  return std::pair{std::move(arg_results), err};
}

type::QualType Compiler::Visit(ast::Call const *node, VerifyTypeTag) {
  auto[arg_results, err] = VerifyFnArgs(this, node->args());
  // TODO handle cyclic dependencies in call arguments.
  if (err) { return type::QualType::Error(); }

  if (auto *b = node->callee()->if_as<ast::BuiltinFn>()) {
    // TODO: Should we allow these to be overloaded?
    ASSIGN_OR(return type::QualType::Error(), auto result,
                     VerifyCall(this, b, node->args(), arg_results));
    return set_result(node, type::QualType(result.type(), result.constant()));
  }

  ASSIGN_OR(return type::QualType::Error(),  //
                   auto os, MakeOverloadSet(this, node->callee(), arg_results));
  ASSIGN_OR(return type::QualType::Error(),  //
                   auto table,
                   FnCallDispatchTable::Verify(this, os, arg_results));
  // TODO might be constant?
  auto result = type::QualType::NonConstant(table.result_type());
  data_.set_dispatch_table(node, std::move(table));
  DEBUG_LOG("dispatch-verify")("Resulting type of dispatch is ", result);
  return set_result(node, result);
}

type::QualType Compiler::Visit(ast::Cast const *node, VerifyTypeTag) {
  auto expr_result = Visit(node->expr(), VerifyTypeTag{});
  auto type_result = Visit(node->type(), VerifyTypeTag{});
  if (not expr_result.ok() or not type_result.ok()) {
    return type::QualType::Error();
  }

  if (type_result.type() != type::Type_) {
    error_log()->CastToNonType(node->span);
    return type::QualType::Error();
  }
  if (not type_result.constant()) {
    error_log()->CastToNonConstantType(node->span);
    return type::QualType::Error();
  }
  auto *t = ASSERT_NOT_NULL(interpretter::EvaluateAs<type::Type const *>(
      MakeThunk(node->type(), type::Type_)));
  if (t->is<type::Struct>()) {
    return VerifyUnaryOverload(this, "as", node, expr_result);
  } else {
    if (not type::CanCast(expr_result.type(), t)) {
      error_log()->InvalidCast(expr_result.type()->to_string(), t->to_string(),
                               node->span);
      NOT_YET("log an error", expr_result.type(), t);
    }

    return set_result(node, type::QualType(t, expr_result.constant()));
  }
}

type::QualType Compiler::Visit(ast::ChainOp const *node, VerifyTypeTag) {
  std::vector<type::QualType> results;
  results.reserve(node->exprs().size());
  for (auto *expr : node->exprs()) {
    results.push_back(Visit(expr, VerifyTypeTag{}));
  }
  if (absl::c_any_of(results,
                     [](type::QualType const &v) { return not v.ok(); })) {
    return type::QualType::Error();
  }

  if (node->ops()[0] == frontend::Operator::Or) {
    bool found_err = false;
    for (size_t i = 0; i < results.size() - 1; ++i) {
      if (results[i].type() == type::Block) {
        if (not results[i].constant()) {
          NOT_YET("log an error: non const block");
        }

        error_log()->EarlyRequiredBlock(node->exprs()[i]->span);
        found_err = true;
      } else {
        goto not_blocks;
      }
    }
    if (found_err) { return type::QualType::Error(); }
    auto &last = results.back();
    if (last.type() != type::Block) {
      goto not_blocks;
    } else if (not results.back().constant()) {
      NOT_YET("log an error: non const block");
    } else {
      return set_result(node, type::QualType::Constant(last.type()));
    }
  }
not_blocks:

  // TODO Can we recover from errors here? Should we?

  // Safe to just check first because to be on the same chain they must all have
  // the same precedence, and ^, &, and | uniquely hold a given precedence.
  switch (node->ops()[0]) {
    case frontend::Operator::Or:
    case frontend::Operator::And:
    case frontend::Operator::Xor: {
      bool failed                       = false;
      bool is_const                     = true;
      type::Type const *first_expr_type = results[0].type();

      for (auto &result : results) {
        // TODO node collection of error messages could be greatly improved.
        if (result.type() != first_expr_type) {
          auto op_str = [node] {
            switch (node->ops()[0]) {
              case frontend::Operator::Or: return "|";
              case frontend::Operator::And: return "&";
              case frontend::Operator::Xor: return "^";
              default: UNREACHABLE();
            }
          }();

          NOT_YET("Log an error");
          is_const &= result.constant();
          failed = true;
        }
      }

      if (failed) { return type::QualType::Error(); }
      return set_result(node, type::QualType(first_expr_type, is_const));
    } break;
    default: {
      bool is_const = results[0].constant();
      ASSERT(node->exprs().size() >= 2u);
      for (size_t i = 0; i + 1 < node->exprs().size(); ++i) {
        type::QualType const &lhs_result = results[i];
        type::QualType const &rhs_result = results[i + 1];
        is_const &= rhs_result.constant();

        // TODO struct is wrong. generally user-defined (could be array of
        // struct too, or perhaps a variant containing a struct?) need to
        // figure out the details here.
        const char *token = nullptr;
        switch (node->ops()[i]) {
          case frontend::Operator::Lt: token = "<"; break;
          case frontend::Operator::Le: token = "<="; break;
          case frontend::Operator::Eq: token = "=="; break;
          case frontend::Operator::Ne: token = "!="; break;
          case frontend::Operator::Ge: token = ">="; break;
          case frontend::Operator::Gt: token = ">"; break;
          default: UNREACHABLE();
        }

        if (lhs_result.type()->is<type::Struct>() or
            lhs_result.type()->is<type::Struct>()) {
          // TODO overwriting type a bunch of times?
          return VerifyBinaryOverload(this, token, node, lhs_result,
                                      rhs_result);
        }

        if (lhs_result.type() != rhs_result.type() and
            not(lhs_result.type()->is<type::Pointer>() and
                rhs_result.type() == type::NullPtr) and
            not(rhs_result.type()->is<type::Pointer>() and
                lhs_result.type() == type::NullPtr)) {
          NOT_YET("Log an error", lhs_result.type()->to_string(),
                  rhs_result.type()->to_string(), node);

        } else {
          auto cmp = Comparator(lhs_result.type());

          switch (node->ops()[i]) {
            case frontend::Operator::Eq:
            case frontend::Operator::Ne: {
              switch (cmp) {
                case Cmp::Order:
                case Cmp::Equality: continue;
                case Cmp::None:
                  error_log()->ComparingIncomparables(
                      lhs_result.type()->to_string(),
                      rhs_result.type()->to_string(),
                      frontend::SourceRange(node->exprs()[i]->span.begin(),
                                            node->exprs()[i + 1]->span.end()));
                  return type::QualType::Error();
              }
            } break;
            case frontend::Operator::Lt:
            case frontend::Operator::Le:
            case frontend::Operator::Ge:
            case frontend::Operator::Gt: {
              switch (cmp) {
                case Cmp::Order: continue;
                case Cmp::Equality:
                case Cmp::None:
                  error_log()->ComparingIncomparables(
                      lhs_result.type()->to_string(),
                      rhs_result.type()->to_string(),
                      frontend::SourceRange(node->exprs()[i]->span.begin(),
                                            node->exprs()[i + 1]->span.end()));
                  return type::QualType::Error();
              }
            } break;
            default: UNREACHABLE("Expecting a ChainOp operator type.");
          }
        }
      }

      return set_result(node, type::QualType(type::Bool, is_const));
    }
  }
}

type::QualType Compiler::Visit(ast::CommaList const *node, VerifyTypeTag) {
  ASSIGN_OR(
      return type::QualType::Error(), auto results,
             VerifyWithoutSetting(
                 this, base::PtrSpan<ast::Expression const>(node->exprs_)));
  std::vector<type::Type const *> ts;
  ts.reserve(results.size());
  bool is_const = true;
  for (auto const &r : results) {
    ts.push_back(r.type());
    is_const &= r.constant();
  }
  return set_result(node, type::QualType(type::Tup(std::move(ts)), is_const));
}

type::QualType Compiler::Visit(ast::Declaration const *node, VerifyTypeTag) {
  // Declarations may have already been computed. Essentially the first time we
  // see an identifier (either a real identifier node, or a declaration, we need
  // to verify the type, but we only want to do node once.
  if (auto *attempt = prior_verification_attempt(node)) { return *attempt; }
  int dk = 0;
  if (node->IsInferred()) { dk = INFER; }
  if (node->IsUninitialized()) {
    dk |= UNINITIALIZED;
  } else if (node->IsCustomInitialized()) {
    dk |= CUSTOM_INIT;
  }
  type::Type const *node_type = nullptr;
  switch (dk) {
    case 0 /* Default initailization */: {
      ASSIGN_OR(return set_result(node, type::QualType::Error()),
                       auto type_expr_result,
                       Visit(node->type_expr(), VerifyTypeTag{}));
      if (not type_expr_result.constant()) {
        // Hmm, not necessarily an error. Example (not necessarily minimal):
        //
        //   S ::= (x: any`T) => struct {
        //     _val: T = x
        //   }
        //
        NOT_YET("log an error", node->DebugString());
        return set_result(node, type::QualType::Error());
      }
      auto *type_expr_type = type_expr_result.type();
      if (type_expr_type == type::Type_) {
        node_type = ASSERT_NOT_NULL(
            set_result(node,
                       type::QualType::Constant(
                           interpretter::EvaluateAs<type::Type const *>(
                               MakeThunk(node->type_expr(), type_expr_type))))
                .type());

        if (not(node->flags() & ast::Declaration::f_IsFnParam) and
            not node_type->IsDefaultInitializable()) {
          error_log()->TypeMustBeInitialized(node->span,
                                             node_type->to_string());
        }

      } else {
        error_log()->NotAType(node->type_expr()->span,
                              type_expr_type->to_string());
        return set_result(node, type::QualType::Error());
      }
    } break;
    case INFER: UNREACHABLE(); break;
    case INFER | CUSTOM_INIT: {
      DEBUG_LOG("Declaration")(node->DebugString());
      ASSIGN_OR(return set_result(node, type::QualType::Error()),
                       auto init_val_result,
                       Visit(node->init_val(), VerifyTypeTag{}));

      auto reason = Inferrable(init_val_result.type());
      if (reason != InferenceFailureReason::Inferrable) {
        error_log()->UninferrableType(reason, node->init_val()->span);
        return set_result(node, type::QualType::Error());
      }

      // TODO initialization, not assignment.
      if (not VerifyAssignment(this, node->span, init_val_result.type(),
                               init_val_result.type())) {
        return set_result(node, type::QualType::Error());
      }

      node_type =
          set_result(
              node, type::QualType(init_val_result.type(),
                                 (node->flags() & ast::Declaration::f_IsConst)))
              .type();
    } break;
    case INFER | UNINITIALIZED: {
      error_log()->UninferrableType(InferenceFailureReason::Hole,
                                    node->init_val()->span);
      if (node->flags() & ast::Declaration::f_IsConst) {
        error_log()->UninitializedConstant(node->span);
      }
      return set_result(node, type::QualType::Error());
    } break;
    case CUSTOM_INIT: {
      auto init_val_result = Visit(node->init_val(), VerifyTypeTag{});
      bool error           = not init_val_result.ok();

      auto *init_val_type   = Visit(node->init_val(), VerifyTypeTag{}).type();
      auto type_expr_result = Visit(node->type_expr(), VerifyTypeTag{});
      auto *type_expr_type  = type_expr_result.type();

      if (type_expr_type == nullptr) {
        error = true;
      } else if (type_expr_type == type::Type_) {
        if (not type_expr_result.constant()) {
          NOT_YET("log an error");
          error = true;
        } else {
          node_type =
              set_result(node,
                         type::QualType::Constant(
                             interpretter::EvaluateAs<type::Type const *>(
                                 MakeThunk(node->type_expr(), type::Type_))))
                  .type();
        }

        // TODO initialization, not assignment. Error messages will be
        // wrong.
        if (node_type != nullptr and init_val_type != nullptr) {
          error |=
              not VerifyAssignment(this, node->span, node_type, init_val_type);
        }
      } else {
        error_log()->NotAType(node->type_expr()->span,
                              type_expr_type->to_string());
        error = true;
      }

      if (error) { return set_result(node, type::QualType::Error()); }
    } break;
    case UNINITIALIZED: {
      ASSIGN_OR(return set_result(node, type::QualType::Error()),
                       auto type_expr_result,
                       Visit(node->type_expr(), VerifyTypeTag{}));
      auto *type_expr_type = type_expr_result.type();
      if (type_expr_type == type::Type_) {
        if (not type_expr_result.constant()) {
          NOT_YET("log an error");
          return set_result(node, type::QualType::Error());
        }
        node_type =
            set_result(node,
                       type::QualType::Constant(
                           interpretter::EvaluateAs<type::Type const *>(
                               MakeThunk(node->type_expr(), type::Type_))))
                .type();
      } else {
        error_log()->NotAType(node->type_expr()->span,
                              type_expr_type->to_string());
        return set_result(node, type::QualType::Error());
      }

      if (node->flags() & ast::Declaration::f_IsConst) {
        error_log()->UninitializedConstant(node->span);
        return set_result(node, type::QualType::Error());
      }

    } break;
    default: UNREACHABLE(dk);
  }

  if (node->id().empty()) {
    if (node_type == type::Module) {
      // TODO check shadowing against other modules?
      // TODO what if no init val is provded? what if not constant?
      node->scope_->embedded_modules_.insert(
          interpretter::EvaluateAs<module::BasicModule const *>(
              MakeThunk(node->init_val(), type::Module)));
      return set_result(node, type::QualType::Constant(type::Module));
    } else {
      NOT_YET(node_type, node->DebugString());
    }
  }

  ASSERT(node_type != nullptr) << node->DebugString();

  // Gather all declarations with the same identifer that are visible in this
  // scope or that are in a scope which for which this declaration would be
  // visible. In other words, look both up and down the scope tree for
  // declarations of this identifier.
  //
  // It's tempting to assume we only need to look in one direction because we
  // would catch any ambiguity at a later time. However this is not correct. For
  // instance, consider this example:
  //
  // ```
  // if (cond) then {
  //   a := 4
  // }
  // a := 3  // Error: Redeclaration of `a`.
  // ```
  //
  // There is a redeclaration of `a` that needs to be caught. However, If we
  // only look towards the root of the scope tree, we will first see `a := 4`
  // which is not ambiguous. Later we will find `a := 3` which should have been
  // found but wasn't due to the fact that we saw the declaration that was
  // further from the root first while processing.
  //
  // The problem can be described mathematically as follows:
  //
  // Define *scope tree order* to be the partial order defined by D1 <= D2 iff
  // D1's path to the scope tree root is a prefix of D2's path to the scope tree
  // root. Define *processing order* to be the order in which nodes have their
  // types verified.
  //
  // The problem is that scope tree order does not have processing order as a
  // linear extension.
  //
  // To fix this particular problem, we need to make sure we check all
  // declarations that may be ambiguous regardless of whether they are above or
  // below `node` on the scope tree. However, we only want to look at the ones
  // which have been previously processed. This can be checked by looking to see
  // if we have saved the result of this declaration. We can also skip out if
  // the result was an error.
  //
  // TODO Skipping out on errors *might* reduce the fidelity of future
  // compilation work by not finding ambiguities that we should have.
  bool failed_shadowing = false;
  type::Typed<ast::Declaration const *> typed_node_decl(node, node_type);
  for (auto const *decl :
       module::AllAccessibleDecls(node->scope_, node->id())) {
    if (decl == node) { continue; }
    auto *r = prior_verification_attempt(decl);
    if (not r) { continue; }
    auto *t = r->type();
    if (not t) { continue; }

    type::Typed<ast::Declaration const *> typed_decl(decl, t);
    if (Shadow(this, typed_node_decl, typed_decl)) {
      failed_shadowing = true;
      error_log()->ShadowingDeclaration(node->span, (*typed_decl)->span);
    }
  }

  if (failed_shadowing) {
    // TODO node may actually overshoot what we want. It may declare the
    // higher-up-the-scope-tree identifier as the shadow when something else on
    // a different branch could find it unambiguously. It's also just a hack
    // from the get-go so maybe we should just do it the right way.
    return set_result(node, type::QualType::Error());
  }

  return VerifySpecialFunctions(this, node, node_type);
}

type::QualType Compiler::Visit(ast::EnumLiteral const *node, VerifyTypeTag) {
  for (auto const &elem : node->elems()) {
    if (auto *decl = elem->if_as<ast::Declaration>()) {
      auto *t = Visit(decl->init_val(), VerifyTypeTag{}).type();
      ASSERT(type::IsIntegral(t) == true);
      // TODO determine what is allowed here and how to generate errors.
    }
  }

  return set_result(node, type::QualType::Constant(type::Type_));
}

type::QualType Compiler::Visit(ast::FunctionLiteral const *node, VerifyTypeTag) {
  for (auto const &p : node->params()) {
    if ((p.value->flags() & ast::Declaration::f_IsConst) or
        not node->param_dep_graph_.at(p.value.get()).empty()) {
      return set_result(node, type::QualType::Constant(type::Generic));
    }
  }

  return VerifyConcreteFnLit(node);
}

type::QualType Compiler::Visit(ast::Identifier const *node, VerifyTypeTag) {
  DEBUG_LOG("Identifier")(node->DebugString());
  for (auto iter = data_.cyc_deps_.begin(); iter != data_.cyc_deps_.end();
       ++iter) {
    if (*iter == node) {
      error_log()->CyclicDependency(
          std::vector<ast::Identifier const *>(iter, data_.cyc_deps_.end()));
      return type::QualType::Error();
    }
  }
  data_.cyc_deps_.push_back(node);
  base::defer d([&] { data_.cyc_deps_.pop_back(); });

  // `node->decl()` is not necessarily null. Because we may call VerifyType many
  // times in multiple contexts, it is null the first time, but not on future
  // iterations.
  //
  // TODO that means we should probably resolve identifiers ahead of
  // type verification, but I think we rely on type information to figure it out
  // for now so you'll have to undo that first.
  if (node->decl() == nullptr) {
    auto potential_decls =
        module::AllDeclsTowardsRoot(node->scope_, node->token());
    DEBUG_LOG("Identifier")(node->DebugString(), ": ", potential_decls);
    switch (potential_decls.size()) {
      case 1: {
        // TODO could it be that evn though there is only one declaration,
        // there's a bound constant of the same name? If so, we need to deal
        // with node case.
        const_cast<ast::Identifier *>(node)->set_decl(potential_decls[0]);
        if (node->decl() == nullptr) { return type::QualType::Error(); }
      } break;
      case 0:
        error_log()->UndeclaredIdentifier(node);
        return type::QualType::Error();
      default:
        // TODO Should we allow the overload?
        error_log()->UnspecifiedOverload(node->span);
        return type::QualType::Error();
    }

    if (not(node->decl()->flags() & ast::Declaration::f_IsConst) and
        node->span.begin() < node->decl()->span.begin()) {
      error_log()->DeclOutOfOrder(node->decl(), node);
    }
  }

  // TODO node is because we may have determined the declartaion previously with
  // a different generic setup but not bound the type for node context. But node
  // is wrong in the sense that the declaration bound is possibly dependent on
  // the context.
  type::Type const *t = type_of(node->decl());

  if (t == nullptr) { return type::QualType::Error(); }
  return set_result(node, type::QualType(t, node->decl()->flags() &
                                              ast::Declaration::f_IsConst));
}

type::QualType Compiler::Visit(ast::Import const *node, VerifyTypeTag) {
  DEBUG_LOG("Import")(node->DebugString());
  ASSIGN_OR(return _, auto result, Visit(node->operand(), VerifyTypeTag{}));
  bool err = false;
  if (result.type() != type::ByteView) {
    // TODO allow (import) overload
    error_log()->InvalidImport(node->operand()->span);
    err = true;
  }

  if (not result.constant()) {
    error_log()->NonConstantImport(node->operand()->span);
    err = true;
  }

  if (err) { return type::QualType::Error(); }
  // TODO storing node might not be safe.
  auto src = interpretter::EvaluateAs<std::string_view>(
      MakeThunk(node->operand(), type::ByteView));
  // TODO source name?
  ASSIGN_OR(
      error_log()->MissingModule(src, std::filesystem::path{"TODO source"});
      return type::QualType::Error(),  //
             auto pending_mod,
             module::ImportModule(std::filesystem::path{src}, module(),
                                  CompileLibraryModule));

  if (not pending_mod.valid()) { return type::QualType::Error(); }
  set_pending_module(node, pending_mod);
  return set_result(node, type::QualType::Constant(type::Module));
}

type::QualType Compiler::Visit(ast::Index const *node, VerifyTypeTag) {
  auto lhs_result = Visit(node->lhs(), VerifyTypeTag{});
  auto rhs_result = Visit(node->rhs(), VerifyTypeTag{});
  if (not lhs_result.ok() or not rhs_result.ok()) {
    return type::QualType::Error();
  }

  auto *index_type = rhs_result.type()->if_as<type::Primitive>();
  if (not index_type or not index_type->is_integral()) {
    error_log()->InvalidIndexType(node->span, lhs_result.type()->to_string(),
                                  lhs_result.type()->to_string());
  }

  if (lhs_result.type() == type::ByteView) {
    return set_result(node, type::QualType(type::Nat8, rhs_result.constant()));
  } else if (auto *lhs_array_type = lhs_result.type()->if_as<type::Array>()) {
    return set_result(
        node, type::QualType(lhs_array_type->data_type, rhs_result.constant()));
  } else if (auto *lhs_buf_type =
                 lhs_result.type()->if_as<type::BufferPointer>()) {
    return set_result(
        node, type::QualType(lhs_buf_type->pointee, rhs_result.constant()));
  } else if (auto *tup = lhs_result.type()->if_as<type::Tuple>()) {
    if (not rhs_result.constant()) {
      error_log()->NonConstantTupleIndex(node->span);
      return type::QualType::Error();
    }

    int64_t index = [&]() -> int64_t {
      auto results = interpretter::Evaluate(MakeThunk(node->rhs(), index_type));
      if (index_type == type::Int8) { return results.get<int8_t>(0).value(); }
      if (index_type == type::Int16) { return results.get<int16_t>(0).value(); }
      if (index_type == type::Int32) { return results.get<int32_t>(0).value(); }
      if (index_type == type::Int64) { return results.get<int64_t>(0).value(); }
      if (index_type == type::Nat8) { return results.get<uint8_t>(0).value(); }
      if (index_type == type::Nat16) {
        return results.get<uint16_t>(0).value();
      }
      if (index_type == type::Nat32) {
        return results.get<uint32_t>(0).value();
      }
      if (index_type == type::Nat64) {
        return results.get<uint64_t>(0).value();
      }
      UNREACHABLE();
    }();

    if (index < 0 or index >= static_cast<int64_t>(tup->size())) {
      error_log()->IndexingTupleOutOfBounds(node->span, tup->to_string(),
                                            tup->size(), index);
      return type::QualType::Error();
    }

    return set_result(
        node, type::QualType(tup->entries_.at(index), lhs_result.constant()));

  } else {
    error_log()->InvalidIndexing(node->span, lhs_result.type()->to_string());
    return type::QualType::Error();
  }
}

type::QualType Compiler::Visit(ast::Goto const *node, VerifyTypeTag) {
  for (auto const &option : node->options()) {
    for (auto const &expr : option.args()) {
      Visit(expr.get(), VerifyTypeTag{});
    }
  }
  return type::QualType::Constant(type::Void());
}

type::QualType Compiler::Visit(ast::Jump const *node, VerifyTypeTag) {
  DEBUG_LOG("Jump")(node->DebugString());
  bool err = false;
  std::vector<type::Type const *> param_types;
  param_types.reserve(node->params().size());
  for (auto const &param : node->params()) {
    auto v = Visit(param.value.get(), VerifyTypeTag{});
    if (not v.ok()) {
      err = true;
    } else {
      param_types.push_back(v.type());
    }
  }

  return set_result(node, err ? type::QualType::Error()
                              : type::QualType::Constant(type::Jmp(param_types)));
}

type::QualType Compiler::Visit(ast::PrintStmt const *node, VerifyTypeTag) {
  auto verify_results = VerifySpan(this, node->exprs());
  if (not verify_results) { return type::QualType::Error(); }
  for (auto &verify_result : *verify_results) {
    auto *expr_type = verify_result.second.type();
    // TODO print arrays?
    if (expr_type->is<type::Primitive>() or expr_type->is<type::Pointer>() or
        expr_type == type::ByteView or expr_type->is<type::Enum>() or
        expr_type->is<type::Flags>()) {
      continue;
    } else {
      ast::OverloadSet os(node->scope_, "print");
      AddAdl(&os, "print", expr_type);
      // TODO using expr.get() for the dispatch table is super janky. node is
      // used so we don't collide with the table for the actual expression as
      // `print f(x)` needs a table both for the printing and for the call to
      // `f`. Test node thoroughly.
      NOT_YET();
    }
  }
  return type::QualType::NonConstant(type::Void());
}

type::QualType Compiler::Visit(ast::ReturnStmt const *node, VerifyTypeTag) {
  auto c = VerifyAndGetConstness(this, node->exprs());
  if (c == Constness::Error) { return type::QualType::Error(); }
  return c == Constness::Const ? type::QualType::Constant(type::Void())
                               : type::QualType::NonConstant(type::Void());
}

type::QualType Compiler::Visit(ast::YieldStmt const *node, VerifyTypeTag) {
  auto c = VerifyAndGetConstness(this, node->exprs());
  if (c == Constness::Error) { return type::QualType::Error(); }
  return c == Constness::Const ? type::QualType::Constant(type::Void())
                               : type::QualType::NonConstant(type::Void());
}

type::QualType Compiler::Visit(ast::ScopeLiteral const *node, VerifyTypeTag) {
  auto verify_result = set_result(node, type::QualType::Constant(type::Scope));
  bool error         = false;
  for (auto const *decl : node->decls()) {
    auto result = Visit(decl, VerifyTypeTag{});
    if (not result.constant()) {
      error = true;
      NOT_YET("log an error");
    }
  }
  // TODO verify that it has at least one entry and exit point each.
  if (error) { return type::QualType::Error(); }
  return verify_result;
}

static absl::flat_hash_map<ir::Jump const *, ir::ScopeDef const *>
MakeJumpInits(Compiler *c, ast::OverloadSet const &os) {
  absl::flat_hash_map<ir::Jump const *, ir::ScopeDef const *> inits;
  DEBUG_LOG("ScopeNode")("Overload set for inits has size ", os.members().size());
  for (ast::Expression const *member : os.members()) {
    DEBUG_LOG("ScopeNode")(member->DebugString());
    auto *def = interpretter::EvaluateAs<ir::ScopeDef *>(
        c->MakeThunk(member, type::Scope));
    DEBUG_LOG("ScopeNode")(def);
    if (def->work_item and *def->work_item) { (std::move(*def->work_item))(); }
    for (auto *init : def->inits_) {
      bool success = inits.emplace(init, def).second;
      static_cast<void>(success);
      ASSERT(success == true);
    }
  }
  return inits;
}

type::QualType Compiler::Visit(ast::ScopeNode const *node, VerifyTypeTag) {
  DEBUG_LOG("ScopeNode")(node->DebugString());

  auto [arg_results, err] = VerifyFnArgs(this, node->args());
  ASSIGN_OR(return type::QualType::Error(),  //
                   auto os, MakeOverloadSet(this, node->name(), arg_results));
  auto inits = MakeJumpInits(this, os);

  DEBUG_LOG("ScopeNode")(inits);

  // TODO handle cyclic dependencies in call arguments.
  if (err) { return type::QualType::Error(); }

  ASSIGN_OR(return type::QualType::Error(),  //
                   auto table,
                   ScopeDispatchTable::Verify(this, node, inits, arg_results));
  data_.set_scope_dispatch_table(node, std::move(table));
  // TODO might be constant? Actually handle yields correctly.
  auto result = type::QualType::NonConstant(type::Void());
  return set_result(node, result);
}

type::QualType Compiler::Visit(ast::StructLiteral const *node, VerifyTypeTag) {
  bool err = false;
  for (auto const &field : node->fields()) {
    type::QualType type_expr_result;
    if (field.type_expr()) {
      type_expr_result = Visit(field.type_expr(), VerifyTypeTag{});
    }

    type::QualType init_val_result;
    if (field.init_val()) {
      init_val_result = Visit(field.init_val(), VerifyTypeTag{});
    }

    if ((field.type_expr() and not type_expr_result) or
        (field.init_val() and not init_val_result)) {
      err = true;
      continue;
    }

    if (field.type_expr() and not type_expr_result.constant()) {
      err = true;
      NOT_YET("Log an error, type must be constant");
    }

    if (field.init_val() and not init_val_result.constant()) {
      err = true;
      NOT_YET("Log an error, type must be constant");
    }

    if (field.init_val() and field.init_val() and
        init_val_result != type_expr_result) {
      err = true;
      NOT_YET("log an error, type mismatch");
    }
  }

  if (err) { return set_result(node, type::QualType::Error()); }
  return set_result(node, type::QualType::Constant(type::Type_));
}

type::QualType Compiler::Visit(ast::ParameterizedStructLiteral const *node, VerifyTypeTag) {
  std::vector<type::Type const *> ts;
  ts.reserve(node->params().size());
  for (auto const &a : node->params()) {
    ts.push_back(Visit(&a, VerifyTypeTag{}).type());
  }
  if (absl::c_any_of(ts, [](type::Type const *t) { return t == nullptr; })) {
    return type::QualType::Error();
  }

  return set_result(node, type::QualType::Constant(
                              type::GenStruct(node->scope_, std::move(ts))));
}

type::QualType Compiler::Visit(ast::StructType const *node, VerifyTypeTag) {
  for (auto &arg : node->args_) { Visit(arg.get(), VerifyTypeTag{}); }
  return set_result(node, type::QualType::Constant(type::Type_));
}

type::QualType Compiler::Visit(ast::Switch const *node, VerifyTypeTag) {
  bool is_const               = true;
  type::Type const *expr_type = nullptr;
  if (node->expr_) {
    ASSIGN_OR(return _, auto result, Visit(node->expr_.get(), VerifyTypeTag{}));
    is_const &= result.constant();
    expr_type = result.type();
  }

  absl::flat_hash_set<type::Type const *> types;
  bool err = false;
  for (auto & [ body, cond ] : node->cases_) {
    auto cond_result = Visit(cond.get(), VerifyTypeTag{});
    auto body_result = Visit(body.get(), VerifyTypeTag{});
    err |= not cond_result or not body_result;
    if (err) {
      NOT_YET();
      continue;
    }

    is_const &= cond_result.constant() and body_result.constant();
    if (node->expr_) {
      static_cast<void>(expr_type);
      // TODO dispatch table
    } else {
      if (cond_result.type() != type::Bool) {
        error_log()->SwitchConditionNeedsBool(cond_result.type()->to_string(),
                                              node->span);
      }
    }
    // TODO if there's an error, an unorderded_set is not helpful for giving
    // good error messages.
    if (body->is<ast::Expression>()) {
      // TODO check that it's actually a jump
      types.insert(body_result.type());
    }
  }
  if (err) { return type::QualType::Error(); }

  // TODO check to ensure that the type is either exhaustable or has a default.

  if (types.empty()) {
    return set_result(node, type::QualType(type::Void(), is_const));
  }
  auto some_type = *types.begin();
  if (absl::c_all_of(types,
                     [&](type::Type const *t) { return t == some_type; })) {
    // TODO node might be a constant.
    return set_result(node, type::QualType(some_type, is_const));
  } else {
    NOT_YET("handle type error");
    return type::QualType::Error();
  }
}

type::QualType Compiler::Visit(ast::Terminal const *node, VerifyTypeTag) {
  return set_result(node,
                    type::QualType::Constant(type::Prim(node->basic_type())));
}

type::QualType Compiler::Visit(ast::Unop const *node, VerifyTypeTag) {
  ASSIGN_OR(return type::QualType::Error(), auto result,
                   Visit(node->operand(), VerifyTypeTag{}));
  auto *operand_type = result.type();

  switch (node->op()) {
    case frontend::Operator::Copy:
      if (not operand_type->IsCopyable()) {
        NOT_YET("log an error. not copyable");
      }
      // TODO Are copies always consts?
      return set_result(node, type::QualType(operand_type, result.constant()));
    case frontend::Operator::Move:
      if (not operand_type->IsMovable()) {
        NOT_YET("log an error. not movable");
      }
      // TODO Are copies always consts?
      return set_result(node, type::QualType(operand_type, result.constant()));
    case frontend::Operator::BufPtr:
      return set_result(node, type::QualType(type::Type_, result.constant()));
    case frontend::Operator::TypeOf:
      return set_result(node, type::QualType(type::Type_, result.constant()));
    case frontend::Operator::Eval:
      if (not result.constant()) {
        // TODO here you could return a correct type and just have there
        // be an error regarding constness. When you do node probably worth a
        // full pass over all verification code.
        error_log()->NonConstantEvaluation(node->operand()->span);
        return type::QualType::Error();
      } else {
        return set_result(node, type::QualType(operand_type, result.constant()));
      }
    case frontend::Operator::Which:
      if (not operand_type->is<type::Variant>()) {
        error_log()->WhichNonVariant(operand_type->to_string(), node->span);
      }
      return set_result(node, type::QualType(type::Type_, result.constant()));
    case frontend::Operator::At:
      if (operand_type->is<type::Pointer>()) {
        return set_result(
            node, type::QualType(operand_type->as<type::Pointer>().pointee,
                               result.constant()));
      } else {
        error_log()->DereferencingNonPointer(operand_type->to_string(),
                                             node->span);
        return type::QualType::Error();
      }
    case frontend::Operator::And:
      // TODO  does it make sense to take the address of a constant? I think it
      // has to but it also has to have some special meaning. Things we take the
      // address of in run-time code need to be made available at run-time.
      return set_result(
          node, type::QualType(type::Ptr(operand_type), result.constant()));
    case frontend::Operator::Mul:
      if (operand_type != type::Type_) {
        NOT_YET("log an error, ", operand_type, node);
        return type::QualType::Error();
      } else {
        return set_result(node, type::QualType(type::Type_, result.constant()));
      }
    case frontend::Operator::Sub:
      if (type::IsNumeric(operand_type)) {
        return set_result(node, type::QualType(operand_type, result.constant()));
      } else if (operand_type->is<type::Struct>()) {
        return VerifyUnaryOverload(this, "-", node, result);
      }
      NOT_YET();
      return type::QualType::Error();
    case frontend::Operator::Expand:
      // NOTE: It doesn't really make sense to ask for the type of an expanded
      // argument, but since we consider the type of the result of a function
      // call returning multiple arguments to be a tuple, we do the same here.
      //
      if (operand_type->is<type::Tuple>()) {
        // TODO there should be a way to avoid copying over any of entire type
        return set_result(node, type::QualType(operand_type, result.constant()));
      } else {
        NOT_YET();  // Log an error. can't expand a non-tuple.
      }
    case frontend::Operator::Not:
      if (operand_type == type::Bool or operand_type->is<type::Enum>() or
          operand_type->is<type::Flags>()) {
        return set_result(node, type::QualType(operand_type, result.constant()));
      }
      if (operand_type->is<type::Struct>()) {
        return VerifyUnaryOverload(this, "!", node, result);
      } else {
        NOT_YET("log an error");
        return type::QualType::Error();
      }
    case frontend::Operator::Needs:
      if (operand_type != type::Bool) {
        error_log()->PreconditionNeedsBool(node->operand()->span,
                                           operand_type->to_string());
      }
      if (not result.constant()) { NOT_YET(); }
      return set_result(node, type::QualType::Constant(type::Void()));
    case frontend::Operator::Ensure:
      if (operand_type != type::Bool) {
        error_log()->PostconditionNeedsBool(node->operand()->span,
                                            operand_type->to_string());
      }
      if (not result.constant()) { NOT_YET(); }
      return set_result(node, type::QualType::Constant(type::Void()));
    case frontend::Operator::VariadicPack: {
      if (not result.constant()) { NOT_YET("Log an error"); }
      // TODO could be a type, or a function returning a ty
      if (result.type() == type::Type_) {
        return set_result(node, type::QualType::Constant(type::Type_));
      } else if (result.type()->is<type::Function>()) {
        NOT_YET();
      }
      NOT_YET(*node);
    }
    default: UNREACHABLE(*node);
  }
}

}  // namespace compiler
