#ifndef ICARUS_BASE_UNTYPED_BUFFER_H
#define ICARUS_BASE_UNTYPED_BUFFER_H

#include <cstring>
#include <string>
#include <utility>

#include "base/debug.h"

namespace base {
struct untyped_buffer {
  untyped_buffer(size_t starting_capacity = 0)
      : size_(0),
        capacity_(starting_capacity),
        data_(static_cast<char *>(malloc(starting_capacity))) {}

  static untyped_buffer MakeFull(size_t starting_size) {
    untyped_buffer result(starting_size);
    result.size_ = result.capacity_;
    return result;
  }

  untyped_buffer(untyped_buffer &&that) noexcept
      : size_(that.size_), capacity_(that.capacity_), data_(that.data_) {
    that.size_     = 0;
    that.capacity_ = 0;
    that.data_     = nullptr;
  }
  untyped_buffer(untyped_buffer const &that) noexcept
      : size_(that.size_),
        capacity_(that.size_),
        data_(static_cast<char *>(malloc(size_))) {
    std::memcpy(data_, that.data_, size_);
  }

  untyped_buffer &operator=(untyped_buffer &&that) noexcept {
    size_     = std::exchange(that.size_, 0);
    capacity_ = std::exchange(that.capacity_, 0);
    data_     = std::exchange(that.data_, nullptr);
    return *this;
  }

  untyped_buffer &operator=(untyped_buffer const &that) noexcept {
    size_     = that.size_;
    capacity_ = that.size_;
    data_     = static_cast<char *>(malloc(capacity_));
    std::memcpy(data_, that.data_, size_);
    return *this;
  }

  ~untyped_buffer() { free(data_); }

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  template <typename T>
  T get(size_t offset) const {
    static_assert(std::is_trivially_copyable_v<T>);
    ASSERT(offset + sizeof(T) <= size_);
    T result{};
    std::memcpy(&result, data_ + offset, sizeof(T));
    return result;
  }

  void *raw(size_t offset) {
    ASSERT(offset <= size_);
    return data_ + offset;
  }

  void const *raw(size_t offset) const {
    ASSERT(offset <= size_);
    return data_ + offset;
  }

  template <typename T>
  void set(size_t offset, T const &t) {
    static_assert(std::is_trivially_copyable_v<T>);
    ASSERT(offset + sizeof(T) <= size_);
    std::memcpy(data_ + offset, &t, sizeof(T));
  }

  template <typename T>
  size_t append(T const &t) {
    size_t old_size_with_alignment = append_bytes(sizeof(T), alignof(T));
    set(old_size_with_alignment, t);
    return old_size_with_alignment;
  }

  void write(size_t offset, base::untyped_buffer const &buf) {
    append_bytes(buf.size(), 1);
    std::memcpy(data_ + offset, buf.data_, buf.size_);
  }

  // Returns an offset to the newly appended region
  size_t append_bytes(size_t num, size_t alignment) {
    // TODO combine with core::FwdAlign?
    size_t old_size_with_alignment = ((size_ - 1) | (alignment - 1)) + 1;
    size_t new_size                = old_size_with_alignment + num;
    if (new_size > capacity_) { reallocate(new_size); }
    size_ = new_size;
    return old_size_with_alignment;
  }

  void pad_to(size_t n) {
    size_t new_size = std::max(n, size_);
    if (new_size > capacity_) { reallocate(new_size); }
    size_ = new_size;
  }

  std::string to_string() const { return to_string(8, 0); }
  std::string to_string(size_t width, size_t indent) const;

 private:
  void reallocate(size_t num) {
    size_t new_cap = std::max<size_t>(num, capacity_ * 2);
    char *new_data = static_cast<char *>(malloc(new_cap));
    std::memcpy(new_data, data_, size_);
    capacity_ = new_cap;
    free(data_);
    data_ = new_data;
  }

  size_t size_     = 0;
  size_t capacity_ = 0;
  char *data_      = 0;
};
}  // namespace base

#endif  // ICARUS_BASE_UNTYPED_BUFFER_H
