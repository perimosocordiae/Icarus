#ifndef ICARUS_TYPE_STRUCT_H
#define ICARUS_TYPE_STRUCT_H

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ast/hashtag.h"
#include "ast/scope/scope.h"
#include "base/lazy.h"
#include "ir/value/native_fn.h"
#include "type/type.h"

namespace ast {
struct StructLiteral;
}  // namespace ast

namespace type {

struct Struct : public Type {
  struct Field {
    explicit Field() = default;
    explicit Field(type::Type const *t) : type(t) {}
    // TODO make a string_view but deal with trickiness of moving

    bool contains_hashtag(ast::Hashtag needle) const {
      for (auto const &h : hashtags_) {
        if (h == needle) { return true; }
      }
      return false;
    }

    std::string name;
    Type const *type = nullptr;
    std::vector<ast::Hashtag> hashtags_;
  };

  Struct(ast::Scope const *scope, std::vector<Field> fields);

  ~Struct() override {}
  void WriteTo(std::string *buf) const override;
  core::Bytes bytes(core::Arch const &arch) const override;
  core::Alignment alignment(core::Arch const &arch) const override;

  bool IsDefaultInitializable() const;
  bool IsCopyable() const;
  bool IsMovable() const;
  bool HasDestructor() const { return false; /* TODO NOT_YET(); */ }

  void Accept(VisitorBase *visitor, void *ret, void *arg_tuple) const override {
    visitor->ErasedVisit(this, ret, arg_tuple);
  }

  // Return the type of a field, or a nullptr if it doesn't exist
  Field const *field(std::string_view name) const;

  module::BasicModule const *defining_module() const { return mod_; }

  core::Bytes offset(size_t n, core::Arch const &arch) const;

  std::vector<Field> const &fields() const { return fields_; }
  size_t index(std::string_view name) const;

  bool contains_hashtag(ast::Hashtag needle) const;

  ast::Scope const *scope_  = nullptr;
  module::BasicModule *mod_ = nullptr;

  base::lazy<ir::NativeFn> init_func_;
  base::lazy<ir::NativeFn> destroy_func_;
  base::lazy<ir::NativeFn> copy_assign_func_;
  base::lazy<ir::NativeFn> move_assign_func_;

  std::vector<ast::Hashtag> hashtags_;
  std::vector<Field> fields_;
  absl::flat_hash_map<std::string_view, size_t> field_indices_;
};

}  // namespace type
#endif  // ICARUS_TYPE_STRUCT_H
