#include "type/struct.h"

#include "ast/ast.h"
#include "ast/hashtag.h"
#include "base/guarded.h"
#include "core/arch.h"
#include "misc/module.h"
#include "type/function.h"
#include "type/pointer.h"

namespace type {
Struct::Struct(
    core::Scope const *scope, ::Module const *mod,
    absl::Span<std::tuple<std::string_view, type::Type const *> const> fields)
    : scope_(scope), mod_(const_cast<::Module *>(mod)) {
  fields_.reserve(fields.size());
  size_t i = 0;
  for (auto [name, t] : fields) {
    field_indices_.emplace(std::string(name), i++);
  }
  for (auto const &[name, t] : fields) {
    Field f(t);
    f.name = field_indices_.find(name)->first;
    fields_.push_back(f);
  }
}

core::Bytes Struct::offset(size_t field_num, core::Arch const &a) const {
  auto offset = core::Bytes{0};
  for (size_t i = 0; i < field_num; ++i) {
    offset += fields_.at(i).type->bytes(a);
    offset = core::FwdAlign(offset, fields_.at(i + 1).type->alignment(a));
  }
  return offset;
}

size_t Struct::index(std::string_view name) const {
  return field_indices_.at(name);
}

Struct::Field const *Struct::field(std::string_view name) const {
  auto iter = field_indices_.find(name);
  if (iter == field_indices_.end()) { return nullptr; }
  return &fields_[iter->second];
}

void Struct::defining_modules(
    absl::flat_hash_set<::Module const *> *modules) const {
  modules->insert(defining_module());
}

void Struct::WriteTo(std::string *result) const {
  result->append("struct.");
  result->append(std::to_string(reinterpret_cast<uintptr_t>(this)));
}

bool Struct::contains_hashtag(ast::Hashtag needle) const {
  for (auto const &tag : hashtags_) {
    if (tag.kind_ == needle.kind_) { return true; }
  }
  return false;
}

core::Bytes Struct::bytes(core::Arch const &a) const {
  auto num_bytes = core::Bytes{0};
  for (auto const &field : fields_) {
    num_bytes += field.type->bytes(a);
    // TODO it'd be in the (common, I think) case where you want both, it would
    // be faster to compute bytes and alignment simultaneously.
    num_bytes = core::FwdAlign(num_bytes, field.type->alignment(a));
  }

  return num_bytes;
}

core::Alignment Struct::alignment(core::Arch const &a) const {
  auto align = core::Alignment{1};
  for (auto const &field : fields_) {
    align = std::max(align, field.type->alignment(a));
  }
  return align;
}

}  // namespace type
