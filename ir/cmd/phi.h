#ifndef ICARUS_IR_CMD_PHI_H
#define ICARUS_IR_CMD_PHI_H

#include "ir/cmd/util.h"

namespace ir {
struct PhiCmd {
  constexpr static cmd_index_t index = 36;

  static std::optional<BlockIndex> Execute(base::untyped_buffer::iterator *iter,
                                           std::vector<Addr> const &ret_slots,
                                           backend::ExecContext *ctx) {
    uint8_t primitive_type = iter->read<uint8_t>();
    uint16_t num           = iter->read<uint16_t>();
    uint64_t index         = std::numeric_limits<uint64_t>::max();
    for (uint16_t i = 0; i < num; ++i) {
      if (ctx->call_stack.top().prev_ == iter->read<BlockIndex>()) {
        index = i;
      }
    }
    ASSERT(index != std::numeric_limits<uint64_t>::max());

    auto &frame = ctx->call_stack.top();
    PrimitiveDispatch(primitive_type, [&](auto tag) {
      using T = typename std::decay_t<decltype(tag)>::type;
      std::vector<T> results = internal::Deserialize<uint16_t, T>(
          iter, [ctx](Reg &reg) { return ctx->resolve<T>(reg); });

      if constexpr(std::is_same_v<T, bool>) {
        frame.regs_.set(GetOffset(frame.fn_, iter->read<Reg>()),
                        bool{results[index]});
      } else {
        frame.regs_.set(GetOffset(frame.fn_, iter->read<Reg>()),
                        results[index]);
      }
    });
    return std::nullopt;
  }

  static std::string DebugString(base::untyped_buffer::const_iterator *iter) {
    NOT_YET();
  }

  static void UpdateForInlining(base::untyped_buffer::iterator *iter,
                                Inliner const &inliner) {
    NOT_YET();
  }
};

template <typename T>
RegOr<T> Phi(Reg r, absl::Span<BlockIndex const> blocks,
             absl::Span<RegOr<T> const> values) {
  ASSERT(blocks.size() == values.size());
  if (values.size() == 1u) { return values[0]; }

  auto &blk = GetBlock();
  blk.cmd_buffer_.append_index<PhiCmd>();
  blk.cmd_buffer_.append(PrimitiveIndex<T>());
  blk.cmd_buffer_.append<uint16_t>(values.size());
  for (auto block : blocks) { blk.cmd_buffer_.append(block); }
  internal::Serialize<uint16_t>(&blk.cmd_buffer_, values);

  Reg result = MakeResult<T>();
  blk.cmd_buffer_.append(result);
  return result;
}

template <typename T>
RegOr<T> Phi(absl::Span<BlockIndex const> blocks,
             absl::Span<RegOr<T> const> values) {
  return Phi(MakeResult<T>(), blocks, values);
}

inline Results Phi(type::Type const *type,
    absl::flat_hash_map<BlockIndex, Results> const & values) {
  if (values.size() == 1) { return values.begin()->second; }
  return type::Apply(type, [&](auto type_holder) {
    using T = typename decltype(type_holder)::type;
    std::vector<RegOr<T>> vals;
    vals.reserve(values.size());
    std::vector<BlockIndex> blocks;
    blocks.reserve(values.size());
    for (auto const &[key, val] : values) {
      blocks.push_back(key);
      vals.push_back(val.template get<T>(0));
    }
    return Results{Phi<T>(blocks, vals)};
  });
}
}  // namespace ir

#endif  // ICARUS_IR_CMD_PHI_H
