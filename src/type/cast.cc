#include "type/cast.h"

#include "type/all.h"

namespace type {
static u8 CastMask(type::Type const *t) {
  //              f32 (0x31) -> f64 (0x33)
  //               ^             ^
  // i8 (0x10) -> i16 (0x11) -> i32 (0x13) -> i64 (0x17)
  //               ^             ^             ^
  //              u8  (0x01) -> u16 (0x03) -> u32 (0x07) -> u64 (0x0f)
  if (t == type::Nat8) { return 0x01; }
  if (t == type::Nat16) { return 0x03; }
  if (t == type::Nat32) { return 0x07; }
  if (t == type::Nat64) { return 0x1f; }
  if (t == type::Int8) { return 0x10; }
  if (t == type::Int16) { return 0x11; }
  if (t == type::Int32) { return 0x13; }
  if (t == type::Int64) { return 0x17; }
  if (t == type::Float32) { return 0x31; }
  if (t == type::Float64) { return 0x33; }
  UNREACHABLE();
}

bool CanCast(type::Type const *from, type::Type const *to) {
  if (from == to) { return true; }
  if (from->is<type::Tuple>() && to == type::Type_) {
    auto const &entries = from->as<type::Tuple>().entries_;
    return std::all_of(entries.begin(), entries.end(),
                       [](type::Type const *t) { return t == type::Type_; });
  }
  if (from->is<type::BufferPointer>() && to->is<type::Pointer>()) {
    return to == from;
  }
  auto from_mask = CastMask(from);
  auto to_mask   = CastMask(to);
  return ((from_mask & to_mask) == from_mask) || CanCastImplicitly(from, to);
}

bool CanCastImplicitly(type::Type const *from, type::Type const *to) {
  return Join(from, to) == to;
}

Type const *Join(Type const *lhs, Type const *rhs) {
  if (lhs == rhs) { return lhs; }
  if (lhs == nullptr) { return rhs; }  // Ignore errors
  if (rhs == nullptr) { return lhs; }  // Ignore errors
  if ((lhs == Block && rhs == OptBlock) || (lhs == OptBlock && rhs == Block)) {
    return Block;
  }
  if (lhs->is<Primitive>() && rhs->is<Primitive>()) {
    return lhs == rhs ? lhs : nullptr;
  }
  if (lhs == NullPtr && rhs->is<Pointer>()) { return rhs; }
  if (rhs == NullPtr && lhs->is<Pointer>()) { return lhs; }
  if (lhs->is<Pointer>() && rhs->is<Pointer>()) {
    return Join(lhs->as<Pointer>().pointee, rhs->as<Pointer>().pointee);
  } else if (lhs->is<Array>() && rhs->is<Array>()) {
    Type const *result = nullptr;
    if (lhs->as<Array>().fixed_length && rhs->as<Array>().fixed_length) {
      if (lhs->as<Array>().len != rhs->as<Array>().len) { return nullptr; }
      result = Join(lhs->as<Array>().data_type, rhs->as<Array>().data_type);
      return result ? Arr(result, lhs->as<Array>().len) : result;
    } else {
      result = Join(lhs->as<Array>().data_type, rhs->as<Array>().data_type);
      return result ? Arr(result) : result;
    }
  } else if (lhs->is<Array>() && rhs == EmptyArray &&
             !lhs->as<Array>().fixed_length) {
    return lhs;
  } else if (rhs->is<Array>() && lhs == EmptyArray &&
             !rhs->as<Array>().fixed_length) {
    return rhs;
  } else if (lhs->is<Variant>()) {
    base::vector<Type const *> rhs_types;
    if (rhs->is<Variant>()) {
      rhs_types = rhs->as<Variant>().variants_;
    } else {
      rhs_types = {rhs};
    }

    auto vars = lhs->as<Variant>().variants_;
    vars.insert(vars.end(), rhs_types.begin(), rhs_types.end());
    return Var(std::move(vars));
  } else if (rhs->is<Variant>()) {  // lhs is not a variant
    // TODO faster lookups? maybe not represented as a vector. at least give
    // a better interface.
    for (Type const *v : rhs->as<Variant>().variants_) {
      if (lhs == v) { return rhs; }
    }
    return nullptr;
  }
  UNREACHABLE(lhs, rhs);
}

// TODO optimize (early exists. don't check lhs->is<> && rhs->is<>. If they
// don't match you can early exit.
Type const *Meet(Type const *lhs, Type const *rhs) {
  if (lhs == rhs) { return lhs; }
  if (lhs == nullptr || rhs == nullptr) { return nullptr; }

  if (lhs == NullPtr || rhs == NullPtr) {
    // TODO It's not obvious to me that this is what I want to do.
    return nullptr;
  }
  if (lhs->is<Pointer>()) {
    return rhs->is<Pointer>() ? Ptr(Meet(lhs->as<Pointer>().pointee,
                                         rhs->as<Pointer>().pointee))
                              : nullptr;
  } else if (lhs->is<Array>() && rhs->is<Array>()) {
    Type const *result = nullptr;
    if (lhs->as<Array>().fixed_length && rhs->as<Array>().fixed_length) {
      if (lhs->as<Array>().len != rhs->as<Array>().len) { return nullptr; }
      result = Meet(lhs->as<Array>().data_type, rhs->as<Array>().data_type);
      return result ? Arr(result, lhs->as<Array>().len) : result;
    } else {
      result = Meet(lhs->as<Array>().data_type, rhs->as<Array>().data_type);
      return result ? Arr(result,
                          std::max(lhs->as<Array>().len, rhs->as<Array>().len))
                    : result;
    }
  } else if (lhs->is<Array>() && rhs == EmptyArray &&
             !lhs->as<Array>().fixed_length) {
    return Arr(lhs->as<Array>().data_type, 0);
  } else if (rhs->is<Array>() && lhs == EmptyArray &&
             !rhs->as<Array>().fixed_length) {
    return Arr(rhs->as<Array>().data_type, 0);
  } else if (lhs->is<Variant>()) {
    // TODO this feels very fishy, cf. ([3; int] | [4; int]) with [--; int]
    base::vector<Type const *> results;
    if (rhs->is<Variant>()) {
      for (Type const *l_type : lhs->as<Variant>().variants_) {
        for (Type const *r_type : rhs->as<Variant>().variants_) {
          Type const *result = Meet(l_type, r_type);
          if (result != nullptr) { results.push_back(result); }
        }
      }
    } else {
      for (Type const *t : lhs->as<Variant>().variants_) {
        if (Type const *result = Meet(t, rhs)) { results.push_back(result); }
      }
    }
    return results.empty() ? nullptr : Var(std::move(results));
  } else if (rhs->is<Variant>()) {  // lhs is not a variant
    // TODO faster lookups? maybe not represented as a vector. at least give a
    // better interface.
    base::vector<Type const *> results;
    for (Type const *t : rhs->as<Variant>().variants_) {
      if (Type const *result = Meet(t, lhs)) { results.push_back(result); }
    }
    return results.empty() ? nullptr : Var(std::move(results));
  }

  return nullptr;
}

}  // namespace type
