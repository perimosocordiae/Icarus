#ifndef ICARUS_TYPE_GENERIC_FUNCTION_H
#define ICARUS_TYPE_GENERIC_FUNCTION_H

#include <string>
#include <vector>

#include "core/arch.h"
#include "core/fn_args.h"
#include "ir/value/value.h"
#include "type/callable.h"
#include "type/function.h"
#include "type/type.h"
#include "type/typed_value.h"

namespace type {

struct GenericFunction : Callable {
  explicit GenericFunction(
      std::function<Function const *(core::FnArgs<Typed<ir::Value>> const &)>
          fn)
      : gen_fn_(std::move(fn)) {}
  ~GenericFunction() override {}
  void WriteTo(std::string *result) const override {
    result->append("generic");
  }

  bool is_big() const override { return false; }

  Function const *concrete(core::FnArgs<Typed<ir::Value>> const &) const;

  std::vector<type::Type const *> return_types(
      core::FnArgs<type::Typed<ir::Value>> const &args) const override;

  void Accept(VisitorBase *visitor, void *ret, void *arg_tuple) const override {
    visitor->ErasedVisit(this, ret, arg_tuple);
  }

  core::Bytes bytes(core::Arch const &arch) const override;
  core::Alignment alignment(core::Arch const &arch) const override;

 private:
  // TODO: Eventually we will want a serializable version of this.
  std::function<Function const *(core::FnArgs<Typed<ir::Value>> const &)>
      gen_fn_;
};

}  // namespace type

#endif  // ICARUS_TYPE_GENERIC_FUNCTION_H
