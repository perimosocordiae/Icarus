#ifndef ICARUS_AST_FN_ARGS_H
#define ICARUS_AST_FN_ARGS_H

namespace AST {
template <typename T> struct FnArgs {
  auto find(const std::string &name) -> decltype(auto) {
    auto iter = named_.begin();
    for (; iter != named_.end(); ++iter) {
      if (iter->first == name) { return iter; }
    }
    return iter;
  }
  auto find(const std::string &name) const -> decltype(auto) {
    auto iter = named_.begin();
    for (; iter != named_.end(); ++iter) {
      if (iter->first == name) { return iter; }
    }
    return iter;
  }

  std::string to_string() const {
    std::string result;
    for (auto &&val : pos_) {
      result += (!val ? "null" : val->to_string()) + ", ";
    }
    for (auto && [ key, val ] : named_) {
      result += key + ": " + (!val ? "null" : val->to_string()) + ", ";
    }
    return result;
  }

  template <typename Fn> void Apply(Fn &&fn) {
    for (auto &&val : pos_) { fn(val); }
    for (auto && [ key, val ] : named_) { fn(val); }
  }
  template <typename Fn>
  auto Transform(Fn &&fn) -> FnArgs<decltype(fn(std::declval<T>()))> {
    FnArgs<decltype(fn(std::declval<T>()))> result;
    result.pos_.reserve(pos_.size());
    for (auto &&val : pos_) { result.pos_.push_back(fn(val)); }
    for (auto && [ key, val ] : named_) { result.named_.emplace(key, fn(val)); }
  }

  std::vector<T> pos_;
  // TODO implement flat map for real
  std::unordered_map<std::string, T> named_;
};



template <typename T>
bool operator<(const FnArgs<T> &lhs, const FnArgs<T> &rhs) {
  if (lhs.pos_.size() != rhs.pos_.size()) {
    return lhs.pos_.size() < rhs.pos_.size();
  }
  if (lhs.named_.size() != rhs.named_.size()) {
    return lhs.named_.size() < rhs.named_.size();
  }
  if (lhs.pos_ < rhs.pos_) { return true; }
  if (lhs.pos_ > rhs.pos_) { return false; }

  auto l_iter = lhs.named_.begin();
  auto r_iter = rhs.named_.begin();
  while (l_iter != lhs.named_.end() && *l_iter == *r_iter) {
    ++l_iter;
    ++r_iter;
  }
  if (l_iter == lhs.named_.end()) { return false; }
  return *l_iter < *r_iter;
}
} // namespace AST

#endif // ICARUS_AST_FN_ARGS_H