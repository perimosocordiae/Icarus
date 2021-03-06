#include "ast/ast.h"
#include "compiler/compiler.h"
#include "compiler/context.h"
#include "compiler/verify/common.h"

namespace compiler {
namespace {

std::optional<type::Quals> VerifyAndGetQuals(
    Compiler *v, base::PtrSpan<ast::Expression const> exprs) {
  bool err          = false;
  type::Quals quals = type::Quals::All();
  for (auto *expr : exprs) {
    auto qt = v->VerifyType(expr)[0];
    err |= not qt.ok();
    if (not err) { quals &= qt.quals(); }
  }
  if (err) { return std::nullopt; }
  return quals;
}

}  // namespace

absl::Span<type::QualType const> Compiler::VerifyType(ast::ArgumentType const *node) {
  return context().set_qual_type(node, type::QualType::Constant(type::Type_));
}

absl::Span<type::QualType const> Compiler::VerifyType(ast::BuiltinFn const *node) {
  return context().set_qual_type(
      node, type::QualType::Constant(ir::Fn(node->value()).type()));
}

absl::Span<type::QualType const> Compiler::VerifyType(ast::ReturnStmt const *node) {
  ASSIGN_OR(return type::QualType::ErrorSpan(),  //
                   auto quals, VerifyAndGetQuals(this, node->exprs()));
  return {};
}

absl::Span<type::QualType const> Compiler::VerifyType(ast::YieldStmt const *node) {
  ASSIGN_OR(return type::QualType::ErrorSpan(),  //
                   auto quals, VerifyAndGetQuals(this, node->exprs()));
  return {};
}

absl::Span<type::QualType const> Compiler::VerifyType(ast::ScopeNode const *node) {
  LOG("ScopeNode", "Verifying ScopeNode named `%s`",
      node->name()->DebugString());
  // TODO: The type of the arguments and the scope name are independent and
  // should not have early-exists.

  ASSIGN_OR(return context().set_qual_type(node, type::QualType::Error()),
                   std::ignore, VerifyArguments(node->args()));

  ASSIGN_OR(return context().set_qual_type(node, type::QualType::Error()),
                   std::ignore, VerifyType(node->name())[0]);

  context().TrackJumps(node);

  for (auto const &block : node->blocks()) { VerifyType(&block); }

  ASSIGN_OR(type::QualType::Error(),  //
            ir::Scope scope, EvaluateOrDiagnoseAs<ir::Scope>(node->name()));
  auto *compiled_scope               = ir::CompiledScope::From(scope);
  ir::OverloadSet &exit_overload_set = compiled_scope->exit();

  std::vector<absl::Span<type::Type const>> return_types;
  std::optional<size_t> num_rets;
  for (core::Arguments<type::QualType> const &args :
       YieldArgumentTypes(context(), node)) {
    if (auto maybe_fn = exit_overload_set.Lookup(args)) {
      auto rets = maybe_fn->type()->return_types();
      if (num_rets) {
        if (*num_rets != rets.size()) { NOT_YET(); }
      } else {
        num_rets = rets.size();
      }
      return_types.push_back(rets);
    } else {
      NOT_YET();
    }
  }

  switch (return_types.size()) {
    case 0:
      return context().set_qual_type(node,
                                     type::QualType::NonConstant(type::Void));
    case 1: {
      std::vector<type::QualType> qts;
      qts.reserve(return_types.front().size());
      for (auto t : return_types.front()) {
        qts.emplace_back(t, type::Quals::Unqualified());
      }
      return context().set_qual_types(node, qts);
    }
    default: {
      std::vector<type::Type> merged_rets(return_types.front().begin(),
                                          return_types.front().end());
      absl::Span<absl::Span<type::Type const> const> return_types_span =
          return_types;
      return_types_span.remove_prefix(1);
      for (absl::Span<type::Type const> rets : return_types_span) {
        for (size_t i = 0; i < *num_rets; ++i) {
          // TODO: Error checking.
          merged_rets[i] = type::Meet(merged_rets[i], rets[i]);
        }
      }

      std::vector<type::QualType> qts;
      qts.reserve(merged_rets.size());
      for (auto t : merged_rets) {
        qts.emplace_back(t, type::Quals::Unqualified());
      }
      return context().set_qual_types(node, qts);
    }
  }
}

absl::Span<type::QualType const> Compiler::VerifyType(ast::Label const *node) {
  return context().set_qual_type(node, type::QualType::Constant(type::Label));
}

}  // namespace compiler
