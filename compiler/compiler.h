#ifndef ICARUS_COMPILER_COMPILER_H
#define ICARUS_COMPILER_COMPILER_H

#include <memory>
#include <optional>
#include <queue>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "ast/ast.h"
#include "ast/overload_set.h"
#include "ast/visitor.h"
#include "base/debug.h"
#include "base/log.h"
#include "base/move_func.h"
#include "compiler/context.h"
#include "compiler/cyclic_dependency_tracker.h"
#include "compiler/module.h"
#include "compiler/transient_state.h"
#include "diagnostic/consumer/consumer.h"
#include "frontend/source/source.h"
#include "ir/builder.h"
#include "ir/instruction/set.h"
#include "ir/interpretter/evaluate.h"
#include "ir/value/addr.h"
#include "ir/value/module_id.h"
#include "ir/value/native_fn.h"
#include "ir/value/reg.h"
#include "ir/value/value.h"
#include "module/importer.h"
#include "module/module.h"
#include "type/array.h"
#include "type/enum.h"
#include "type/flags.h"
#include "type/function.h"
#include "type/generic_function.h"
#include "type/generic_struct.h"
#include "type/jump.h"
#include "type/opaque.h"
#include "type/parameter_pack.h"
#include "type/pointer.h"
#include "type/primitive.h"
#include "type/qual_type.h"
#include "type/struct.h"
#include "type/tuple.h"
#include "type/type.h"
#include "type/visitor.h"

namespace compiler {
struct EmitRefTag {};
struct EmitCopyInitTag {};
struct EmitMoveInitTag {};
struct EmitValueTag {};
struct VerifyTypeTag {};
struct VerifyBodyTag {};
struct EmitAssignTag {};
struct EmitDestroyTag {};
struct EmitDefaultInitTag {};
struct EmitCopyAssignTag {};
struct EmitMoveAssignTag {};

// These are the steps in a traditional compiler of verifying types and emitting
// code. They're tied together because they don't necessarily happen in a
// particular order. Certainly for any given AST node we need to verify its type
// before emitting code for it. However, we may need to emit and execute code
// for some nodes to compute a type at compile-time. For this reason these steps
// require the same contextual data and therefore should be placed into a single
// visitor type.
//
// Note that tying these together has a cost. C++ ties together these steps as
// well as parsing and it has compile-time performance cost as well as language
// semantic restrictions. This design was not chosen lightly. We believe that
// the primary problem with C++ is that parsing is lumped together with
// type-verification and code generation and that the primary benefits that
// surface from separation are from separating parsing from these two stages
// rather than separating all stages. In time we will see if this belief holds
// water.

struct Compiler
    : ast::Visitor<EmitMoveInitTag,
                   void(absl::Span<type::Typed<ir::RegOr<ir::Addr>> const>)>,
      ast::Visitor<EmitCopyInitTag,
                   void(absl::Span<type::Typed<ir::RegOr<ir::Addr>> const>)>,
      ast::Visitor<EmitAssignTag,
                   void(absl::Span<type::Typed<ir::RegOr<ir::Addr>> const>)>,
      ast::Visitor<EmitRefTag, ir::RegOr<ir::Addr>()>,
      ast::Visitor<EmitValueTag, ir::Value()>,
      ast::Visitor<VerifyTypeTag, type::QualType()>,
      ast::Visitor<VerifyBodyTag, WorkItem::Result()>,
      type::Visitor<EmitDestroyTag, void(ir::Reg)>,
      type::Visitor<EmitDefaultInitTag, void(ir::Reg)>,
      type::Visitor<EmitMoveAssignTag,
                    void(ir::RegOr<ir::Addr>, type::Typed<ir::Value> const &)>,
      type::Visitor<EmitCopyAssignTag,
                    void(ir::RegOr<ir::Addr>, type::Typed<ir::Value> const &)> {
  // Resources and pointers/references to data that are guaranteed to outlive
  // any Compiler construction.
  struct PersistentResources {
    ir::Builder &builder;
    Context &data;
    diagnostic::DiagnosticConsumer &diagnostic_consumer;
    module::Importer &importer;
  };
  PersistentResources resources() { return resources_; }

  void VerifyAll(base::PtrSpan<ast::Node const> nodes) {
    for (ast::Node const *node : nodes) {
      if (auto const *decl = node->if_as<ast::Declaration>()) {
        if (decl->flags() & ast::Declaration::f_IsConst) { VerifyType(node); }
      }
    }

    for (ast::Node const *node : nodes) {
      if (auto const *decl = node->if_as<ast::Declaration>()) {
        if (decl->flags() & ast::Declaration::f_IsConst) { continue; }
      }

      VerifyType(node);
    }

    CompleteWorkQueue();
  }
  void CompleteWorkQueue() {
    while (not state_.work_queue.empty()) {
      state_.work_queue.ProcessOneItem();
    }
  }

  void Enqueue(WorkItem work_item) {
    state_.work_queue.Enqueue(std::move(work_item));
  }

  type::QualType VerifyType(ast::Node const *node) {
    return ast::Visitor<VerifyTypeTag, type::QualType()>::Visit(node);
  }

  WorkItem::Result VerifyBody(ast::Node const *node) {
    return ast::Visitor<VerifyBodyTag, WorkItem::Result()>::Visit(node);
  }

  ir::Value EmitValue(ast::Node const *node) {
    return ast::Visitor<EmitValueTag, ir::Value()>::Visit(node);
  }

  void EmitAssign(ast::Node const *node,
                  absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs) {
    ast::Visitor<
        EmitAssignTag,
        void(absl::Span<type::Typed<ir::RegOr<ir::Addr>> const>)>::Visit(node,
                                                                         regs);
  }

  void EmitCopyInit(ast::Node const *node,
                    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs) {
    ast::Visitor<
        EmitCopyInitTag,
        void(absl::Span<type::Typed<ir::RegOr<ir::Addr>> const>)>::Visit(node,
                                                                         regs);
  }

  void EmitMoveInit(ast::Node const *node,
                    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs) {
    ast::Visitor<
        EmitMoveInitTag,
        void(absl::Span<type::Typed<ir::RegOr<ir::Addr>> const>)>::Visit(node,
                                                                         regs);
  }

  ir::RegOr<ir::Addr> EmitRef(ast::Node const *node) {
    return ast::Visitor<EmitRefTag, ir::RegOr<ir::Addr>()>::Visit(node);
  }

  void EmitDestroy(type::Typed<ir::Reg> r) {
    type::Visitor<EmitDestroyTag, void(ir::Reg)>::Visit(r.type().get(),
                                                        r.get());
  }

  void EmitDefaultInit(type::Typed<ir::Reg> r) {
    type::Visitor<EmitDefaultInitTag, void(ir::Reg)>::Visit(r.type().get(),
                                                            r.get());
  }

  void EmitMoveAssign(type::Typed<ir::RegOr<ir::Addr>> const &to,
                      type::Typed<ir::Value> const &from) {
    type::Visitor<EmitMoveAssignTag,
                  void(ir::RegOr<ir::Addr>,
                       type::Typed<ir::Value> const &)>::Visit(to.type().get(),
                                                               to.get(), from);
  }

  void EmitCopyAssign(type::Typed<ir::RegOr<ir::Addr>> const &to,
                      type::Typed<ir::Value> const &from) {
    type::Visitor<EmitCopyAssignTag,
                  void(ir::RegOr<ir::Addr>,
                       type::Typed<ir::Value> const &)>::Visit(to.type().get(),
                                                               to.get(), from);
  }

  explicit Compiler(PersistentResources const &resources);

  // Returns a new `Compiler` instance which points to the same persistent
  // resources.
  Compiler WithPersistent() const;

  Context &context() const { return resources_.data; }
  // TODO: Deprecated. Use `context()`.
  Context &data() const { return resources_.data; }
  ir::Builder &builder() { return resources_.builder; };
  diagnostic::DiagnosticConsumer &diag() const {
    return resources_.diagnostic_consumer;
  }
  module::Importer &importer() const { return resources_.importer; }

  template <typename T>
  base::expected<T, interpretter::EvaluationFailure> EvaluateAs(
      ast::Expression const *expr) {
    ASSIGN_OR(return _.error(), auto val,
                     Evaluate(type::Typed<ast::Expression const *>(
                         expr, type::Get<T>())));
    return val.template get<T>();
  }

  // Evaluates `expr` in the current context as a value of type `T`. If
  // evaluation succeeds, returns the vaule, otherwise adds a diagnostic for the
  // failure and returns `nullopt`. If the expresison is no tof type `T`, the
  // behavior is undefined.
  template <typename T>
  std::optional<T> EvaluateOrDiagnoseAs(ast::Expression const *expr) {
    auto maybe_value =
        Evaluate(type::Typed<ast::Expression const *>(expr, type::Get<T>()));
    if (not maybe_value) {
      diag().Consume(maybe_value.error());
      return std::nullopt;
    }
    return maybe_value->template get<T>();
  }

  ir::Value EvaluateOrDiagnose(type::Typed<ast::Expression const *> expr) {
    if (auto maybe_value = Evaluate(expr)) {
      return *maybe_value;
    } else {
      diag().Consume(maybe_value.error());
      return ir::Value();
    }
  }

  base::expected<ir::Value, interpretter::EvaluationFailure> Evaluate(
      type::Typed<ast::Expression const *> expr, bool must_complete = true);

  core::Params<std::pair<ir::Value, type::QualType>> ComputeParamsFromArgs(
      ast::ParameterizedExpression const *node,
      core::FnArgs<type::Typed<ir::Value>> const &args);

  Context::InsertSubcontextResult Instantiate(
      ast::ParameterizedExpression const *node,
      core::FnArgs<type::Typed<ir::Value>> const &args) {
    return context().InsertSubcontext(node, ComputeParamsFromArgs(node, args));
  }

  std::optional<type::QualType> qual_type_of(ast::Expression const *expr) const;
  type::Type type_of(ast::Expression const *expr) const;

  absl::Span<TransientState::ScopeLandingState const> scope_landings() const {
    return state_.scope_landings;
  }
  void add_scope_landing(TransientState::ScopeLandingState state) {
    state_.scope_landings.push_back(std::move(state));
  }
  void pop_scope_landing() { state_.scope_landings.pop_back(); }

  ir::NativeFn AddFunc(
      type::Function const *fn_type,
      core::Params<type::Typed<ast::Declaration const *>> params);

  void CompleteDeferredBodies();

#define ICARUS_AST_NODE_X(name)                                                \
  type::QualType VerifyType(ast::name const *node);                            \
  type::QualType Visit(VerifyTypeTag, ast::name const *node) override {        \
    return VerifyType(node);                                                   \
  }                                                                            \
                                                                               \
  WorkItem::Result Visit(VerifyBodyTag, ast::name const *node) override {      \
    return VerifyBody(node);                                                   \
  }                                                                            \
                                                                               \
  ir::Value EmitValue(ast::name const *node);                                  \
  ir::Value Visit(EmitValueTag, ast::name const *node) override {              \
    return EmitValue(node);                                                    \
  }
#include "ast/node.xmacro.h"
#undef ICARUS_AST_NODE_X

#define ICARUS_TYPE_TYPE_X(name)                                               \
  void Visit(EmitDestroyTag, type::name const *t, ir::Reg reg) override {      \
    EmitDestroy(type::Typed<ir::Reg, type::name>(reg, t));                     \
  }

#include "type/type.xmacro.h"
#undef ICARUS_TYPE_TYPE_X

  TransientState::YieldedArguments EmitBlockNode(ast::BlockNode const *node);

  // The reason to separate out type/body verification is if the body might
  // transitively have identifiers referring to a declaration that is assigned
  // directly to this node.
  WorkItem::Result VerifyBody(ast::Expression const *node) {
    return WorkItem::Result::Success;
  }
  WorkItem::Result VerifyBody(ast::EnumLiteral const *node);
  WorkItem::Result VerifyBody(ast::FunctionLiteral const *node);
  WorkItem::Result VerifyBody(ast::ParameterizedStructLiteral const *node);
  WorkItem::Result VerifyBody(ast::StructLiteral const *node);

  ir::RegOr<ir::Addr> EmitRef(ast::Access const *node);
  ir::RegOr<ir::Addr> Visit(EmitRefTag, ast::Access const *node) override {
    return EmitRef(node);
  }
  ir::RegOr<ir::Addr> EmitRef(ast::Identifier const *node);
  ir::RegOr<ir::Addr> Visit(EmitRefTag, ast::Identifier const *node) override {
    return EmitRef(node);
  }
  ir::RegOr<ir::Addr> EmitRef(ast::Index const *node);
  ir::RegOr<ir::Addr> Visit(EmitRefTag, ast::Index const *node) override {
    return EmitRef(node);
  }
  ir::RegOr<ir::Addr> EmitRef(ast::UnaryOperator const *node);
  ir::RegOr<ir::Addr> Visit(EmitRefTag,
                            ast::UnaryOperator const *node) override {
    return EmitRef(node);
  }

  void EmitDestroy(type::Typed<ir::Reg, type::Struct> reg);
  void EmitDestroy(type::Typed<ir::Reg, type::Tuple> reg);
  void EmitDestroy(type::Typed<ir::Reg, type::Array> reg);
  void EmitDestroy(type::Typed<ir::Reg, type::Primitive> reg);
  void EmitDestroy(type::Typed<ir::Reg, type::Pointer> reg);
  void EmitDestroy(type::Typed<ir::Reg, type::BufferPointer> reg);

#define DEFINE_EMIT_ASSIGN(T)                                                  \
  void Visit(EmitCopyAssignTag, T const *ty, ir::RegOr<ir::Addr> r,            \
             type::Typed<ir::Value> const &tv) override {                      \
    EmitCopyAssign(type::Typed<ir::RegOr<ir::Addr>, T>(r, ty), tv);            \
  }                                                                            \
  void EmitCopyAssign(type::Typed<ir::RegOr<ir::Addr>, T> const &,             \
                      type::Typed<ir::Value> const &);                         \
                                                                               \
  void Visit(EmitMoveAssignTag, T const *ty, ir::RegOr<ir::Addr> r,            \
             type::Typed<ir::Value> const &tv) override {                      \
    EmitMoveAssign(type::Typed<ir::RegOr<ir::Addr>, T>(r, ty), tv);            \
  }                                                                            \
  void EmitMoveAssign(type::Typed<ir::RegOr<ir::Addr>, T> const &r,            \
                      type::Typed<ir::Value> const &);

  DEFINE_EMIT_ASSIGN(type::Array);
  DEFINE_EMIT_ASSIGN(type::Enum);
  DEFINE_EMIT_ASSIGN(type::Flags);
  DEFINE_EMIT_ASSIGN(type::Function);
  DEFINE_EMIT_ASSIGN(type::Pointer);
  DEFINE_EMIT_ASSIGN(type::BufferPointer);
  DEFINE_EMIT_ASSIGN(type::Primitive);
  DEFINE_EMIT_ASSIGN(type::Struct);
  DEFINE_EMIT_ASSIGN(type::Tuple);

#undef DEFINE_EMIT_ASSIGN

#define DEFINE_EMIT_DEFAULT_INIT(T)                                            \
  void Visit(EmitDefaultInitTag, T const *ty, ir::Reg r) override {            \
    EmitDefaultInit(type::Typed<ir::Reg, T>(r, ty));                           \
  }                                                                            \
  void EmitDefaultInit(type::Typed<ir::Reg, T> const &r);

  DEFINE_EMIT_DEFAULT_INIT(type::Array);
  DEFINE_EMIT_DEFAULT_INIT(type::Flags);
  DEFINE_EMIT_DEFAULT_INIT(type::Pointer);
  DEFINE_EMIT_DEFAULT_INIT(type::Primitive);
  DEFINE_EMIT_DEFAULT_INIT(type::Struct);
  DEFINE_EMIT_DEFAULT_INIT(type::Tuple);

#undef DEFINE_EMIT_DEFAULT_INIT

  void EmitAssign(ast::Access const *node,
                  absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs);
  void Visit(EmitAssignTag, ast::Access const *node,
             absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs) override {
    return EmitAssign(node, regs);
  }

  void EmitAssign(ast::ArrayType const *node,
                  absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs);
  void Visit(EmitAssignTag, ast::ArrayType const *node,
             absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs) override {
    return EmitAssign(node, regs);
  }

  void EmitAssign(ast::BinaryOperator const *node,
                  absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs);
  void Visit(EmitAssignTag, ast::BinaryOperator const *node,
             absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs) override {
    return EmitAssign(node, regs);
  }

  void EmitAssign(ast::Call const *node,
                  absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs);
  void Visit(EmitAssignTag, ast::Call const *node,
             absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs) override {
    return EmitAssign(node, regs);
  }

  void EmitAssign(ast::Identifier const *node,
                  absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs);
  void Visit(EmitAssignTag, ast::Identifier const *node,
             absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs) override {
    return EmitAssign(node, regs);
  }

  void EmitAssign(ast::Index const *node,
                  absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs);
  void Visit(EmitAssignTag, ast::Index const *node,
             absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs) override {
    return EmitAssign(node, regs);
  }

  void EmitAssign(ast::Terminal const *node,
                  absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs);
  void Visit(EmitAssignTag, ast::Terminal const *node,
             absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs) override {
    return EmitAssign(node, regs);
  }

  void EmitAssign(ast::UnaryOperator const *node,
                  absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs);
  void Visit(EmitAssignTag, ast::UnaryOperator const *node,
             absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs) override {
    return EmitAssign(node, regs);
  }

#define DEFINE_EMIT_INIT(node_type)                                            \
  void EmitCopyInit(node_type const *node,                                     \
                    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs);  \
  void Visit(EmitCopyInitTag, node_type const *node,                           \
             absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs)          \
      override {                                                               \
    return EmitCopyInit(node, regs);                                           \
  }                                                                            \
  void EmitMoveInit(node_type const *node,                                     \
                    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs);  \
  void Visit(EmitMoveInitTag, node_type const *node,                           \
             absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> regs)          \
      override {                                                               \
    return EmitMoveInit(node, regs);                                           \
  }
  DEFINE_EMIT_INIT(ast::Access)
  DEFINE_EMIT_INIT(ast::ArrayLiteral)
  DEFINE_EMIT_INIT(ast::ArrayType)
  DEFINE_EMIT_INIT(ast::BinaryOperator)
  DEFINE_EMIT_INIT(ast::Call)
  DEFINE_EMIT_INIT(ast::Cast)
  DEFINE_EMIT_INIT(ast::DesignatedInitializer)
  DEFINE_EMIT_INIT(ast::Identifier)
  DEFINE_EMIT_INIT(ast::Index)
  DEFINE_EMIT_INIT(ast::Terminal)
  DEFINE_EMIT_INIT(ast::UnaryOperator)
#undef DEFINE_EMIT_INIT

  void EmitMoveInit(type::Typed<ir::Value> from_val,
                    type::Typed<ir::Reg> to_var) {
    auto to_type = to_var.type()->as<type::Pointer>().pointee();
    // TODO Optimize once you understand the semantics better.
    if (to_type->IsDefaultInitializable()) { EmitDefaultInit(to_var); }

    EmitMoveAssign(type::Typed<ir::RegOr<ir::Addr>>(to_var.get(), to_type),
                   from_val);
  }

  void EmitCopyInit(type::Typed<ir::Value> from_val,
                    type::Typed<ir::Reg> to_var) {
    auto to_type = to_var.type()->as<type::Pointer>().pointee();
    // TODO Optimize once you understand the semantics better.
    if (to_type->IsDefaultInitializable()) { EmitDefaultInit(to_var); }

    EmitCopyAssign(type::Typed<ir::RegOr<ir::Addr>>(to_var.get(), to_type),
                   from_val);
  }

  type::QualType VerifyBinaryOverload(std::string_view symbol,
                                      ast::Expression const *node,
                                      type::Typed<ir::Value> const &lhs,
                                      type::Typed<ir::Value> const &rhs);

  // TODO: Figure out where to stick these.
  struct CallError {
    struct TooManyArguments {
      size_t num_provided;
      size_t max_num_accepted;
    };

    struct MissingNonDefaultableArguments {
      absl::flat_hash_set<std::string> names;
    };

    struct TypeMismatch {
      std::variant<std::string, size_t> parameter;
      type::Type argument_type;
    };

    struct NoParameterNamed {
      std::string name;
    };

    struct PositionalArgumentNamed {
      size_t index;
      std::string name;
    };

    using ErrorReason =
        std::variant<TooManyArguments, MissingNonDefaultableArguments,
                     TypeMismatch, NoParameterNamed, PositionalArgumentNamed,
                     CallError>;

    // TODO: It might be better to track back to the definition, but for now all
    // we have is type information.
    absl::flat_hash_map<type::Callable const *, ErrorReason> reasons;
  };

  ir::ModuleId EvaluateModuleWithCache(ast::Expression const *expr);

  std::optional<core::Params<type::QualType>> VerifyParams(
      core::Params<std::unique_ptr<ast::Declaration>> const &params);

 private:
  friend struct WorkItem;

  WorkItem::Result CompleteStruct(ast::StructLiteral const *node);

  std::optional<core::FnArgs<type::Typed<ir::Value>>> VerifyFnArgs(
      core::FnArgs<ast::Expression const *> const &args);

  type::QualType VerifyUnaryOverload(char const *symbol,
                                     ast::Expression const *node,
                                     type::Typed<ir::Value> const &operand);

  base::expected<type::QualType, CallError> VerifyCall(
      ast::Call const *call_expr,
      absl::flat_hash_map<ast::Expression const *, type::Callable const *> const
          &overload_map,
      core::FnArgs<type::Typed<ir::Value>> const &args);

  std::pair<type::QualType, absl::flat_hash_map<ast::Expression const *,
                                                type::Callable const *>>
  VerifyCallee(ast::Expression const *callee,
               core::FnArgs<type::Typed<ir::Value>> const &args);

  PersistentResources resources_;
  TransientState state_;

  // TODO: Should be persistent, but also needs on some local context
  // (Context).
  CyclicDependencyTracker cylcic_dependency_tracker_;
};

inline void WorkQueue::ProcessOneItem() {
  ASSERT(items_.empty() == false);
  WorkItem item = std::move(items_.front());
  items_.pop();
  WorkItem::Result result = item.Process();
  bool deferred           = (result == WorkItem::Result::Deferred);
  if (deferred) { items_.push(std::move(item)); }
#if defined(ICARUS_DEBUG)
  if (deferred) {
    LOG("", "Deferring %s", item.node->DebugString());
    cycle_breaker_count_ = deferred ? cycle_breaker_count_ + 1 : 0;
  }
  ASSERT(cycle_breaker_count_ <= items_.size());
#endif
}

}  // namespace compiler

#endif  // ICARUS_COMPILER_COMPILER_H
