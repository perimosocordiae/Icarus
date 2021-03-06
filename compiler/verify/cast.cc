#include "ast/ast.h"
#include "compiler/compiler.h"
#include "compiler/type_for_diagnostic.h"
#include "compiler/verify/common.h"
#include "type/primitive.h"

namespace compiler {
namespace {

struct CastToNonConstantType {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "cast-to-non-constant-type";

  diagnostic::DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text(
            "Cannot cast to a type which is not declared constant."),
        diagnostic::SourceQuote(src).Highlighted(range, diagnostic::Style{}));
  }

  frontend::SourceRange range;
};

}  // namespace

absl::Span<type::QualType const> Compiler::VerifyType(ast::Cast const *node) {
  auto expr_qt = VerifyType(node->expr())[0];
  auto type_qt = VerifyType(node->type())[0];
  if (not expr_qt.ok() or not type_qt.ok()) {
    return context().set_qual_type(node, type::QualType::Error());
  }

  if (type_qt.type() != type::Type_) {
    diag().Consume(NotAType{.range = node->range(), .type = type_qt.type()});
    return context().set_qual_type(node,type::QualType::Error());
  }
  if (not type_qt.constant()) {
    diag().Consume(CastToNonConstantType{.range = node->range()});
    return context().set_qual_type(node, type::QualType::Error());
  }

  ASSIGN_OR(return context().set_qual_type(node, type::QualType::Error()),  //
                   auto t, EvaluateOrDiagnoseAs<type::Type>(node->type()));
  if (not type::CanCastExplicitly(expr_qt.type(), t)) {
    diag().Consume(InvalidCast{
        .from  = TypeForDiagnostic(node->expr(), context()),
        .to    = TypeForDiagnostic(node, context()),
        .range = node->range(),
    });
  }

  return context().set_qual_type(
      node, type::QualType(t, expr_qt.quals() & ~type::Quals::Buf()));
}

}  // namespace compiler
