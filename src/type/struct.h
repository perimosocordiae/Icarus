#ifndef ICARUS_TYPE_STRUCT_H
#define ICARUS_TYPE_STRUCT_H

#include <mutex>
#include <string>
#include <string_view>
#include "base/container/vector.h"
#include "ir/val.h"
#include "type/struct_data.h"
#include "type/type.h"

struct Architecture;

namespace ast {
struct StructLiteral;
}  // namespace ast

namespace ir {
struct Func;
}  // namespace ir

namespace type {
struct IncompleteStruct;

struct Struct : public Type {
  using Field = StructData::Field;

  ~Struct() override {}
  BASIC_METHODS;

  // Return the type of a field, or a nullptr if it doesn't exist
  Field const *field(std::string const &name) const;

  bool needs_destroy() const override {
    return std::any_of(data_.fields_.begin(), data_.fields_.end(),
                       [](Field const &f) { return f.type->needs_destroy(); });
  }

  size_t offset(size_t n, Architecture const &arch) const;

  base::vector<Field> const &fields() const { return data_.fields_; }
  size_t index(std::string const &name) const;

 private:
  friend struct IncompleteStruct;
  Struct(StructData &&data) : data_(std::move(data)) {}

  StructData data_;
  mutable std::mutex mtx_;
  mutable ir::Func *init_func_ = nullptr, *assign_func_ = nullptr,
                   *destroy_func_ = nullptr, *repr_func_ = nullptr;
};

}  // namespace type
#endif  // ICARUS_TYPE_STRUCT_H
