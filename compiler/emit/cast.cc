#include "ast/ast.h"
#include "compiler/compiler.h"
#include "ir/value/char.h"

namespace compiler {

void Compiler::EmitCopyInit(
    ast::Cast const *node,
    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  ASSERT(to.size() == 1u);
  auto t = context().qual_type(node).type();
  EmitCopyAssign(to[0], type::Typed<ir::Value>(EmitValue(node), t));
}

void Compiler::EmitMoveInit(
    ast::Cast const *node,
    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  ASSERT(to.size() == 1u);
  auto t = context().qual_type(node).type();
  EmitMoveAssign(type::Typed<ir::RegOr<ir::Addr>>(*to[0], t),
                 type::Typed<ir::Value>(EmitValue(node), t));
}

ir::Value Compiler::EmitValue(ast::Cast const *node) {
  type::Type to_type = context().qual_type(node).type();
  auto values        = EmitValue(node->expr());

  if (to_type == type::Type_) { return ir::Value(values.get<type::Type>()); }

  auto from_type = context().qual_type(node->expr()).type();

  if (to_type == from_type) { return values; }

  if (to_type == type::Char) {
    ASSERT((from_type == type::U8 or from_type == type::I8) == true);
    return ir::Value(
        builder().CastTo<ir::Char>(type::Typed<ir::Value>(values, from_type)));
  } else if (from_type == type::Char) {
    ASSERT(type::IsIntegral(to_type) == true);
    return type::ApplyTypes<int8_t, int16_t, int32_t, int64_t, uint8_t,
                            uint16_t, uint32_t, uint64_t>(
        to_type, [&]<typename T>() {
          return ir::Value(
              builder().CastTo<T>(type::Typed<ir::Value>(values, from_type)));
        });
  } else if (type::IsNumeric(from_type)) {
    if (type::IsIntegral(from_type)) {
      return type::ApplyTypes<int8_t, int16_t, int32_t, int64_t, uint8_t,
                              uint16_t, uint32_t, uint64_t, float, double>(
          to_type, [&]<typename T>() {
            return ir::Value(
                builder().CastTo<T>(type::Typed<ir::Value>(values, from_type)));
          });
    } else {
      return type::ApplyTypes<float, double>(to_type, [&]<typename T>() {
        return ir::Value(
            builder().CastTo<T>(type::Typed<ir::Value>(values, from_type)));
      });
    }
  } else if (from_type == type::NullPtr) {
    return ir::Value(ir::Addr::Null());
  } else if (auto const *enum_type = from_type.if_as<type::Enum>()) {
    return type::ApplyTypes<int8_t, int16_t, int32_t, int64_t, uint8_t,
                            uint16_t, uint32_t, uint64_t>(
        to_type, [&]<typename T>() {
          return ir::Value(
              builder().CastTo<T>(type::Typed<ir::Value>(values, enum_type)));
        });
  } else if (auto const *flags_type = from_type.if_as<type::Flags>()) {
    return type::ApplyTypes<int8_t, int16_t, int32_t, int64_t, uint8_t,
                            uint16_t, uint32_t, uint64_t>(
        to_type, [&]<typename T>() {
          return ir::Value(
              builder().CastTo<T>(type::Typed<ir::Value>(values, from_type)));
        });
  } else {
    NOT_YET(from_type, " to ", to_type);
  }
}

void Compiler::EmitMoveAssign(
    ast::Cast const *node,
    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  ASSERT(to.size() == 1u);
  auto t = context().qual_type(node).type();
  EmitMoveAssign(to[0], type::Typed<ir::Value>(EmitValue(node), t));
}

void Compiler::EmitCopyAssign(
    ast::Cast const *node,
    absl::Span<type::Typed<ir::RegOr<ir::Addr>> const> to) {
  ASSERT(to.size() == 1u);
  auto t = context().qual_type(node).type();
  EmitCopyAssign(to[0], type::Typed<ir::Value>(EmitValue(node), t));
}

}  // namespace compiler
