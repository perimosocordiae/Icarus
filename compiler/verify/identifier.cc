#include "ast/ast.h"
#include "base/defer.h"
#include "compiler/compiler.h"
#include "compiler/cyclic_dependency_tracker.h"
#include "type/overload_set.h"
#include "type/qual_type.h"
#include "type/typed_value.h"

namespace compiler {

namespace {

struct DeclOutOfOrder {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "declaration-out-of-order";

  diagnostic::DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Variable `%s` used before it was declared.", id),
        diagnostic::SourceQuote(src)
            .Highlighted(use_range, diagnostic::Style::ErrorText())
            .Highlighted(id_range, diagnostic::Style::ErrorText()));
  }

  std::string_view id;
  frontend::SourceRange id_range;
  frontend::SourceRange use_range;
};

struct UndeclaredIdentifier {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "undeclared-identifier";

  diagnostic::DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Found an undeclared identifier '%s':", id),
        diagnostic::SourceQuote(src).Highlighted(
            range, diagnostic::Style::ErrorText()));
  }

  std::string_view id;
  frontend::SourceRange range;
};

struct UncapturedIdentifier {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "uncaptured-identifier";

  diagnostic::DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text("Found an identifier '%s' which is not visible in the "
                         "current scope:",
                         id),
        diagnostic::SourceQuote(src).Highlighted(
            range, diagnostic::Style::ErrorText()));
  }

  std::string_view id;
  frontend::SourceRange range;
};

struct NonCallableInOverloadSet {
  static constexpr std::string_view kCategory = "type-error";
  static constexpr std::string_view kName     = "non-callable-in-overload-set";

  // TODO this assumes a single source for all references in this message.
  diagnostic::DiagnosticMessage ToMessage(frontend::Source const *src) const {
    return diagnostic::DiagnosticMessage(
        diagnostic::Text(
            "Non-callable type `%s` in overload set requested here:",
            decl_type),
        diagnostic::SourceQuote(src).Highlighted(
            id, diagnostic::Style::ErrorText()),
        diagnostic::Text("Declaration here:"),
        diagnostic::SourceQuote(src).Highlighted(decl, diagnostic::Style{}));
  }

  frontend::SourceRange id;
  frontend::SourceRange decl;
  type::Type decl_type;
};

}  // namespace

type::QualType Compiler::VerifyType(ast::Identifier const *node) {
  if (context().cyclic_error(node)) { return type::QualType::Error(); }

  // Dependency pushed until `token` is destroyed.
  auto token =
      cylcic_dependency_tracker_.PushDependency(node, context(), diag());
  if (not token) { return type::QualType::Error(); }

  // TODO: In what circumstances could this have been seen more than once?
  if (auto const *qt = context().qual_type(node)) {
    LOG("Identifier", "Already saw `%s` so returning %s.", node->name(), *qt);
    return *qt;
  }

  type::QualType qt;

  std::vector<ast::Declaration::Id const *> potential_decl_ids =
      module::AllVisibleDeclsTowardsRoot(node->scope(), node->name());

  // Ensure the types of all potential declarations hav ealready been verified.
  // TODO: Eventually we may want to relax this for functions where we don't
  // need the entire decl we just need to know if it's callable.
  bool error = false;
  for (auto const*id : potential_decl_ids) {
    if (context().qual_type(&id->declaration())) { continue; }
    auto qt = VerifyType(&id->declaration());
    if (not qt.ok()) { error = true; }
  }
  if (error) { return type::QualType::Error(); }

  LOG("Identifier", "%s: %p %s", node->DebugString(), node, potential_decl_ids);
  switch (potential_decl_ids.size()) {
    case 1: {
      auto const *id   = potential_decl_ids[0];
      auto const *decl = &id->declaration();
      if (decl->flags() & ast::Declaration::f_IsConst) {
        if (auto const *maybe_qt = context().qual_type(id)) {
          qt = *maybe_qt;
        } else {
          ASSIGN_OR(return type::QualType::Error(), qt, VerifyType(id));
        }

      } else {
        if (node->range().begin() < id->range().begin()) {
          diag().Consume(DeclOutOfOrder{
              .id        = node->name(),
              .id_range  = potential_decl_ids[0]->range(),
              .use_range = node->range(),
          });
          // Haven't seen the declaration yet, so we can't proceed.
          return type::QualType::Error();
        } else {
          qt = *ASSERT_NOT_NULL(context().qual_type(id));
        }

        if (not qt.constant()) {
          if (qt.type().is<type::Array>()) {
            qt = type::QualType(qt.type(), qt.quals() | type::Quals::Buf());
          }

          // TODO: shouldn't need to reconstruct just to set the quals.
          qt = type::QualType(qt.type(), qt.quals() | type::Quals::Ref());
        }
      }

      if (qt.type().is<type::Callable>()) {
        context().SetAllOverloads(node, ast::OverloadSet(potential_decl_ids));
      }

      LOG("Identifier", "setting %s", node->name());
      // TOOD: Support multiple declarations
      std::vector<ast::Declaration const *> decls;
      for (auto const *id : potential_decl_ids) {
        decls.push_back(&id->declaration());
      }
      context().set_decls(node, std::move(decls));
    } break;
    case 0: {
      // TODO: Performance. We don't need to look at these, we just need to know
      // if any exist.
      if (module::AllDeclsTowardsRoot(node->scope(), node->name()).empty()) {
        diag().Consume(UndeclaredIdentifier{
            .id    = node->name(),
            .range = node->range(),
        });
      } else {
        diag().Consume(UncapturedIdentifier{
            .id    = node->name(),
            .range = node->range(),
        });
      }
      return type::QualType::Error();
    } break;
    default: {
      type::Quals quals = type::Quals::Const();
      absl::flat_hash_set<type::Callable const *> member_types;
      bool error = false;

      for (auto const *id : potential_decl_ids) {
        qt = *context().qual_type(id);
        if (not qt.ok() or qt.HasErrorMark()) {
          return type::QualType::Error();
        }
      }

      for (auto const *id : potential_decl_ids) {
        qt = *context().qual_type(id);
        if (not qt.ok()) { return type::QualType::Error(); }

        if (auto *c = qt.type().if_as<type::Callable>()) {
          quals &= qt.quals();
          member_types.insert(c);
        } else {
          diag().Consume(NonCallableInOverloadSet{
              .id        = node->range(),
              .decl      = id->range(),
              .decl_type = qt.type(),
          });
          error = true;
        }
      }

      if (error) { return type::QualType::Error(); }

      context().SetAllOverloads(node, ast::OverloadSet(potential_decl_ids));
      qt =
          type::QualType(type::MakeOverloadSet(std::move(member_types)), quals);
      LOG("Identifier", "setting %s", node->name());
      // TOOD: Support multiple declarations
      std::vector<ast::Declaration const *> decls;
      for (auto const *id : potential_decl_ids) {
        decls.push_back(&id->declaration());
      }
      context().set_decls(node, std::move(decls));
    } break;
  }

  ASSERT(qt.type().valid() == true);
  LOG("Identifier", "setting %s: %s", node->name(), qt);
  return context().set_qual_type(node, qt);
}

}  // namespace compiler
