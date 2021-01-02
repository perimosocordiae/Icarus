#ifndef ICARUS_IR_VALUE_ADDR_H
#define ICARUS_IR_VALUE_ADDR_H

#include <string>

#include "base/log.h"
#include "core/bytes.h"

namespace ir {
struct Addr {
  enum class Kind : uint8_t { Heap, Stack, ReadOnly };

  constexpr Addr() : data_(0) {}
  constexpr static Addr Null() { return Addr{}; }

  constexpr static Addr ReadOnly(uint64_t index) {
    Addr addr;
    addr.data_ = (index << 2) + static_cast<uint8_t>(Kind::ReadOnly);
    return addr;
  }

  static Addr Heap(void *ptr) {
    Addr addr;
    addr.data_ = (reinterpret_cast<uintptr_t>(ptr) << 2) +
                 static_cast<uint8_t>(Kind::Heap);
    return addr;
  }

  constexpr static Addr Stack(uint64_t index) {
    Addr addr;
    addr.data_ = (index << 2) + static_cast<uint8_t>(Kind::Stack);
    return addr;
  }

  constexpr Addr &operator+=(core::Bytes b) {
    data_ += (b.value() << 2);
    return *this;
  }

  auto operator<=>(Addr const &) const = default;

  friend core::Bytes operator-(Addr lhs, Addr rhs) {
    return core::Bytes(((lhs.data_ >> 2) | (lhs.data_ & 0b11) << 62) -
                       ((rhs.data_ >> 2) | (rhs.data_ & 0b11) << 62));
  }

  template <typename H>
  friend H AbslHashValue(H h, Addr a) {
    return H::combine(std::move(h), a.data_);
  }

  constexpr Kind kind() const { return static_cast<Kind>(data_ & 0b11); }
  constexpr uint64_t stack() const { return data_ >> 2; }
  constexpr uint64_t rodata() const { return data_ >> 2; }
  void *heap() const { return reinterpret_cast<void *>(data_ >> 2); }

  std::string to_string() const;

  friend std::ostream &operator<<(std::ostream &os, Addr addr) {
    return os << addr.to_string();
  }

 private:
  uintptr_t data_;
};

inline Addr operator+(Addr a, core::Bytes b) { return a += b; }
inline Addr operator+(core::Bytes b, Addr a) { return a += b; }


std::string stringify(Addr::Kind k);

}  // namespace ir

#endif  // ICARUS_IR_VALUE_ADDR_H
