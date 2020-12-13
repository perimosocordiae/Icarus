#include "ast/ast.h"
#include "compiler/compiler.h"
#include "compiler/emit/common.h"
#include "ir/value/value.h"
#include "type/struct.h"
#include "type/type.h"

namespace compiler {
namespace {

void CompleteBody(Compiler &compiler,
                  ast::ParameterizedStructLiteral const *node, type::Type t) {
  NOT_YET();
}

}  // namespace

ir::Value Compiler::EmitValue(ast::ParameterizedStructLiteral const *node) {
  // TODO: Check the result of body verification.
  if (context().ShouldVerifyBody(node)) { VerifyBody(node); }
  return ir::Value(ir::GenericFn(
      [c = this->WithPersistent(),
       node](core::Arguments<type::Typed<ir::Value>> const &args) mutable
      -> ir::NativeFn {
        return MakeConcreteFromGeneric<ast::ParameterizedStructLiteral,
                                       CompleteBody>(c, node, args);
      }));
}

WorkItem::Result Compiler::CompleteStruct(
    ast::ParameterizedStructLiteral const *node) {
  LOG("struct", "Completing struct-literal emission: %p must-complete = %s",
      node, state_.must_complete ? "true" : "false");

  type::Struct *s = context().get_struct(node);
  if (s->completeness() == type::Completeness::Complete) {
    LOG("struct", "Already complete, exiting: %p", node);
    return WorkItem::Result::Success;
  }

  ASSIGN_OR(return WorkItem::Result::Failure,  //
                   auto fn, StructCompletionFn(*this, s, node->fields()));
  // TODO: What if execution fails.
  interpreter::Execute(std::move(fn));
  s->complete();
  LOG("struct", "Completed %s which is a struct %s with %u field(s).",
      node->DebugString(), *s, s->fields().size());
  return WorkItem::Result::Success;
}

}  // namespace compiler