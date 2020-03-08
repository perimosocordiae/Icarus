#ifndef ICARUS_TYPE_FUNCTION_H
#define ICARUS_TYPE_FUNCTION_H

#include <cstring>

#include "core/params.h"
#include "type/type.h"
#include "type/typed_value.h"

namespace type {

struct Function : public Type {
  TYPE_FNS(Function);
  Function(core::Params<Type const *> in, std::vector<Type const *> out)
      : params_(std::move(in)), output_(std::move(out)) {
#if defined(ICARUS_DEBUG)
    for (auto const &p : params_) { ASSERT(p.value != nullptr); }
    for (auto *t : output_) { ASSERT(t != nullptr); }
#endif  // defined(ICARUS_DEBUG)
  }

  void Accept(VisitorBase *visitor, void *ret, void *arg_tuple) const override {
    visitor->ErasedVisit(this, ret, arg_tuple);
  }

  bool is_big() const override { return false; }

  core::Params<Type const *> const &params() const { return params_; }
  absl::Span<Type const *const> output() const { return output_; }

 private:
  // Each `Param<Type const*>` has a `std::string_view` member representing the
  // parameter name. This is viewing an identifier owned by a declaration in the
  // syntax tree which means it is valid for the lifetime of the syntax tree.
  // However, this type is never destroyed, so it's lifetime is indeed longer
  // than that of the syntax tree.
  //
  // TODO either fix this, or come up with a simple and robust rule we can
  // follow to ensure this is safe. Do we keep the syntax tree around for the
  // lifetime of the program? Any program?
  core::Params<Type const *> params_;
  std::vector<Type const *> output_;
};

Function const *Func(core::Params<Type const *> in,
                     std::vector<Type const *> out);

}  // namespace type

#endif  // ICARUS_TYPE_FUNCTION_H
