#ifndef ICARUS_BASE_META_H
#define ICARUS_BASE_META_H

#include <cstdint>
#include <type_traits>
#include <utility>

namespace base {

template <typename T>
struct DeductionBlocker {
  using type = T;
};
template <typename T>
using DeductionBlockerT = typename DeductionBlocker<T>::type;

template <typename... Ts>
struct first;

template <typename T, typename... Ts>
struct first<T, Ts...> {
  using type = T;
};

template <typename... Ts>
using first_t = typename first<Ts...>::type;

template <typename T>
struct identity {
  using type = T;
};
template <typename T>
using identity_t = typename identity<T>::type;

template <typename... Ts>
using type_list = void(Ts...);

namespace internal {

template <typename T, typename U>
struct type_list_pair_catter;
template <typename... P1s, typename... P2s>
struct type_list_pair_catter<type_list<P1s...>, type_list<P2s...>> {
  using type = type_list<P1s..., P2s...>;
};

template <typename... Ts>
struct type_list_catter {
  using type = type_list<>;
};
template <typename T, typename... Ts>
struct type_list_catter<T, Ts...>
    : type_list_pair_catter<T, typename type_list_catter<Ts...>::type> {};

}  // namespace internal

template <typename... TLs>
using type_list_cat = typename ::base::internal::type_list_catter<TLs...>::type;

template <typename T>
constexpr bool always_false() {
  return false;
}

struct MetaValue {
  template <typename H>
  friend H AbslHashValue(H h, MetaValue m) {
    return H::combine(std::move(h), m.value_);
  }

  friend constexpr bool operator==(MetaValue lhs, MetaValue rhs) {
    return lhs.value_ == rhs.value_;
  }

  friend constexpr bool operator!=(MetaValue lhs, MetaValue rhs) {
    return not(lhs == rhs);
  }

  template <typename T>
  friend struct Meta;

 private:
  explicit constexpr MetaValue(uintptr_t val) : value_(val) {}
  uintptr_t value_;
};

template <typename T>
struct Meta {
  static MetaValue value() {
    return MetaValue{reinterpret_cast<uintptr_t>(data_)};
  }
  /* implicit */ operator MetaValue() { return value(); }

 private:
  static void const* const data_;
};

template <typename T>
void const* const Meta<T>::data_ = &Meta<T>::data_;

template <typename T>
Meta<T> meta;

template <typename Lhs, typename Rhs>
constexpr bool operator==(Meta<Lhs>, Meta<Rhs>) {
  return std::is_same_v<Lhs, Rhs>;
}

template <typename Lhs, typename Rhs>
constexpr bool operator!=(Meta<Lhs> lhs, Meta<Rhs> rhs) {
  return not(lhs == rhs);
}

}  // namespace base

#endif  // ICARUS_BASE_META_H
