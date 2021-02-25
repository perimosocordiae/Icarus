#ifndef ICARUS_IR_VALUE_MODULE_ID_H
#define ICARUS_IR_VALUE_MODULE_ID_H

#include <cstddef>
#include <memory>

#include "base/extend.h"
#include "base/extend/absl_format.h"
#include "base/extend/absl_hash.h"
#include "base/flyweight_map.h"
#include "base/global.h"
#include "frontend/source/file_name.h"

namespace ir {
namespace internal_module {

template <typename ModuleType>
struct ModuleData {
  base::flyweight_map<frontend::CanonicalFileName> ids;
  std::vector<std::unique_ptr<ModuleType>> modules;
};

template <typename ModuleType>
inline base::Global<ModuleData<ModuleType>> all_modules;

}  // namespace internal_module

// A value-type representing a module (unit of compilation).
struct ModuleId : base::Extend<ModuleId, 1>::With<base::AbslHashExtension,
                                                  base::AbslFormatExtension> {
  static constexpr std::string_view kAbslFormatString = "ModuleId(%u)";

  constexpr ModuleId() : id_(std::numeric_limits<size_t>::max()) {}
  constexpr explicit ModuleId(size_t n) : id_(n) {}

  static constexpr ModuleId Invalid() {
    return ModuleId(std::numeric_limits<size_t>::max());
  }

  template <typename ModuleType>
  static std::tuple<ModuleId, ModuleType*, bool> FromFile(
      frontend::CanonicalFileName const& filename) {
    auto handle   = internal_module::all_modules<ModuleType>.lock();
    size_t id     = handle->ids.get(filename);
    bool inserted = false;
    if (id == handle->modules.size()) {
      inserted = true;
      handle->modules.push_back(std::make_unique<ModuleType>());
    }
    ASSERT(id < handle->modules.size());
    return std::tuple(ModuleId(id), handle->modules.back().get(), inserted);
  }

  template <typename ModuleType>
  frontend::CanonicalFileName const& filename() const {
    return internal_module::all_modules<ModuleType>.lock()->ids.get(id_);
  }

  template <typename ModuleType>
  ModuleType const* get() const {
    auto handle = internal_module::all_modules<ModuleType>.lock();
    return handle->modules[id_].get();
  }

 private:
  friend base::EnableExtensions;

  size_t id_;
};

}  // namespace ir

#endif  // ICARUS_IR_VALUE_MODULE_ID_H
