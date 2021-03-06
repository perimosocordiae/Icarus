#include "absl/cleanup/cleanup.h"
#include "ast/ast.h"
#include "compiler/compiler.h"
#include "frontend/lex/operators.h"
#include "ir/value/value.h"
#include "type/interface/ir.h"
#include "type/pointer.h"

namespace compiler {

ir::Value Compiler::EmitValue(ast::UnaryOperator const *node) {
  // TODO: user-defined-types
  switch (node->kind()) {
    case ast::UnaryOperator::Kind::Copy: {
      auto operand_type = context().qual_types(node->operand())[0].type();
      auto reg          = builder().TmpAlloca(operand_type);
      EmitCopyInit(
          type::Typed<ir::Reg>(reg, operand_type),
          type::Typed<ir::Value>(EmitValue(node->operand()), operand_type));
      return ir::Value(builder().PtrFix(reg, operand_type));
    } break;
    case ast::UnaryOperator::Kind::Destroy: {
      EmitDestroy(
          type::Typed<ir::Reg>(EmitValue(node->operand()).get<ir::Reg>(),
                               context().qual_types(node->operand())[0].type()));
      return ir::Value();
    } break;
    case ast::UnaryOperator::Kind::Init:
      // TODO: Not entirely sure this is what the semantics ought to be.
    case ast::UnaryOperator::Kind::Move: {
      auto operand_type = context().qual_types(node->operand())[0].type();
      auto reg = builder().TmpAlloca(operand_type);
      EmitMoveInit(
          type::Typed<ir::Reg>(reg, operand_type),
          type::Typed<ir::Value>(EmitValue(node->operand()), operand_type));
      return ir::Value(builder().PtrFix(reg, operand_type));
    } break;
    case ast::UnaryOperator::Kind::BufferPointer: {
      absl::Cleanup c = [b = state_.must_complete, this] {
        state_.must_complete = b;
      };
      state_.must_complete = false;

      return ir::Value(current_block()->Append(type::BufPtrInstruction{
          .operand = EmitValue(node->operand()).get<ir::RegOr<type::Type>>(),
          .result  = builder().CurrentGroup()->Reserve(),
      }));
    } break;
    case ast::UnaryOperator::Kind::Not: {
      auto operand_qt = context().qual_types(node->operand())[0];
      if (operand_qt.type() == type::Bool) {
        return ir::Value(
            builder().Not(EmitValue(node->operand()).get<ir::RegOr<bool>>()));
      } else if (auto const *t = operand_qt.type().if_as<type::Flags>()) {
        return ir::Value(current_block()->Append(type::XorFlagsInstruction{
            .lhs = EmitValue(node->operand())
                       .get<ir::RegOr<type::Flags::underlying_type>>(),
            .rhs    = t->All,
            .result = builder().CurrentGroup()->Reserve()}));
      } else {
        // TODO: Operator overloading
        NOT_YET();
      }
    } break;
    case ast::UnaryOperator::Kind::Negate: {
      auto operand_ir = EmitValue(node->operand());
      return ApplyTypes<int8_t, int16_t, int32_t, int64_t, float, double>(
          context().qual_types(node->operand())[0].type(), [&]<typename T>() {
            return ir::Value(builder().Neg(operand_ir.get<ir::RegOr<T>>()));
          });
    } break;
    case ast::UnaryOperator::Kind::TypeOf:
      return ir::Value(context().qual_types(node->operand())[0].type());
    case ast::UnaryOperator::Kind::Address:
      return ir::Value(EmitRef(node->operand()));
    case ast::UnaryOperator::Kind::Pointer: {
      absl::Cleanup c = [b = state_.must_complete, this] {
        state_.must_complete = b;
      };
      state_.must_complete = false;

      return ir::Value(current_block()->Append(type::PtrInstruction{
          .operand = EmitValue(node->operand()).get<ir::RegOr<type::Type>>(),
          .result  = builder().CurrentGroup()->Reserve(),
      }));
    } break;
    case ast::UnaryOperator::Kind::At: {
      return builder().Load(
          EmitValue(node->operand()).get<ir::RegOr<ir::addr_t>>(),
          context().qual_types(node)[0].type());
    }
    default: UNREACHABLE("Operator is ", static_cast<int>(node->kind()));
  }
}

void Compiler::EmitCopyInit(
    ast::UnaryOperator const *node,
    absl::Span<type::Typed<ir::RegOr<ir::addr_t>> const> to) {
  switch (node->kind()) {
    case ast::UnaryOperator::Kind::Init:
      EmitCopyInit(node->operand(), to);
      break;
    case ast::UnaryOperator::Kind::Move:
      EmitMoveInit(node->operand(), to);
      break;
    case ast::UnaryOperator::Kind::Copy:
      EmitCopyInit(node->operand(), to);
      break;
    case ast::UnaryOperator::Kind::Destroy:
      EmitDestroy(type::Typed<ir::Reg>(
          EmitValue(node->operand()).get<ir::Reg>(),
          context().qual_types(node->operand())[0].type()));
      break;
    default: {
      auto from_val = EmitValue(node);
      auto from_qt  = *context().qual_types(node)[0];
      if (to.size() == 1) {
        EmitCopyAssign(to[0], type::Typed<ir::Value>(from_val, from_qt.type()));
      } else {
        NOT_YET();
      }
    } break;
  }
}

void Compiler::EmitMoveInit(
    ast::UnaryOperator const *node,
    absl::Span<type::Typed<ir::RegOr<ir::addr_t>> const> to) {
  switch (node->kind()) {
    case ast::UnaryOperator::Kind::Init:
      EmitMoveInit(node->operand(), to);
      break;
    case ast::UnaryOperator::Kind::Move:
      EmitMoveInit(node->operand(), to);
      break;
    case ast::UnaryOperator::Kind::Copy:
      EmitCopyInit(node->operand(), to);
      break;
    default: {
      auto from_val = EmitValue(node);
      auto from_qt  = *context().qual_types(node)[0];
      if (to.size() == 1) {
        EmitMoveAssign(to[0], type::Typed<ir::Value>(from_val, from_qt.type()));
      } else {
        NOT_YET();
      }
    } break;
  }
}

ir::Reg Compiler::EmitRef(ast::UnaryOperator const *node) {
  ASSERT(node->kind() == ast::UnaryOperator::Kind::At);
  return EmitValue(node->operand()).get<ir::Reg>();
}

// TODO: Unit tests
void Compiler::EmitCopyAssign(
    ast::UnaryOperator const *node,
    absl::Span<type::Typed<ir::RegOr<ir::addr_t>> const> to) {
  switch (node->kind()) {
    case ast::UnaryOperator::Kind::Init: {
      EmitCopyInit(node->operand(), to);
    } break;
    case ast::UnaryOperator::Kind::Copy: {
      EmitCopyAssign(node->operand(), to);
    } break;
    case ast::UnaryOperator::Kind::Move:
      EmitMoveAssign(node->operand(), to);
      break;
    default: {
      auto from_val = EmitValue(node);
      auto from_qt  = *context().qual_types(node)[0];
      if (to.size() == 1) {
        EmitMoveAssign(to[0], type::Typed<ir::Value>(from_val, from_qt.type()));
      } else {
        NOT_YET();
      }
    } break;
  }
}

// TODO: Unit tests
void Compiler::EmitMoveAssign(
    ast::UnaryOperator const *node,
    absl::Span<type::Typed<ir::RegOr<ir::addr_t>> const> to) {
  switch (node->kind()) {
    case ast::UnaryOperator::Kind::Init: {
      EmitMoveInit(node->operand(), to);
    } break;
    case ast::UnaryOperator::Kind::Copy: {
      EmitCopyAssign(node->operand(), to);
    } break;
    case ast::UnaryOperator::Kind::Move:
      EmitMoveAssign(node->operand(), to);
      break;
    default: {
      auto from_val = EmitValue(node);
      auto from_qt  = *context().qual_types(node)[0];
      if (to.size() == 1) {
        EmitMoveAssign(to[0], type::Typed<ir::Value>(from_val, from_qt.type()));
      } else {
        NOT_YET();
      }
    } break;
  }
}

bool Compiler::PatternMatch(ast::UnaryOperator const *node,
                            PatternMatchingContext &pmc) {
  switch (node->kind()) {
    case ast::UnaryOperator::Kind::Pointer: {
      auto t = pmc.value.get<type::Type>(0);
      if (not t.is<type::Pointer>() or t.is<type::BufferPointer>()) {
        return false;
      }
      pmc.value.set(0, type::Type(t.as<type::Pointer>().pointee()));
      return PatternMatch(node->operand(), pmc);
    } break;
    case ast::UnaryOperator::Kind::BufferPointer: {
      auto t        = pmc.value.get<type::Type>(0);
      auto const *p = t.if_as<type::BufferPointer>();
      if (not p) { return false; }
      pmc.value.set(0, type::Type(p->pointee()));
      return PatternMatch(node->operand(), pmc);
    } break;
    case ast::UnaryOperator::Kind::Not: {
      bool b = pmc.value.get<bool>(0);
      pmc.value.set(0, not b);
      return PatternMatch(node->operand(), pmc);
    } break;
    case ast::UnaryOperator::Kind::Negate: {
      auto t        = context().qual_types(node->operand())[0].type();
      auto const *p = t.if_as<type::Primitive>();
      if (not p) { return false; }
      p->Apply([&]<typename T>() {
        if constexpr (std::is_arithmetic_v<T>) {
          T x = pmc.value.template get<T>(0);
          pmc.value.set(0, -x);
        }
      });
      return PatternMatch(node->operand(), pmc);
    } break;
    case ast::UnaryOperator::Kind::Address: NOT_YET();
    case ast::UnaryOperator::Kind::Copy:
    case ast::UnaryOperator::Kind::Init:
    case ast::UnaryOperator::Kind::Move:
      return PatternMatch(node->operand(), pmc);
    default: UNREACHABLE();
  }
}

}  // namespace compiler
