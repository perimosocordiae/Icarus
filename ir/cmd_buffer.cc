#include "ir/cmd_buffer.h"

#include <string_view>
#include <type_traits>

#include "backend/exec.h"
#include "base/debug.h"
#include "ir/cmd/basic.h"
#include "ir/cmd/cast.h"
#include "ir/cmd/jumps.h"
#include "ir/cmd/load.h"
#include "ir/cmd/misc.h"
#include "ir/cmd/print.h"
#include "ir/cmd/register.h"
#include "ir/cmd/set_ret.h"
#include "ir/cmd/store.h"
#include "ir/cmd/types.h"
#include "ir/compiled_fn.h"

namespace ir {

std::optional<BlockIndex> LegacyCmd::Execute(
    base::untyped_buffer::iterator* iter,
    std::vector<ir::Addr> const& ret_slots, backend::ExecContext* ctx) {
  auto block = ctx->ExecuteCmd(*iter->read<Cmd*>(), ret_slots);
  if (block == ir::BlockIndex{-2}) { return std::nullopt; }
  return block;
}

void LegacyCmd::UpdateForInlining(base::untyped_buffer::iterator* iter,
                                  Inliner const& inliner) {
  auto& cmd = *iter->read<Cmd*>();
  switch (cmd.op_code_) {
    case Op::Death: UNREACHABLE();
    case Op::JumpPlaceholder: {
      NOT_YET();
    } break;

    case Op::GetRet: NOT_YET();
    case Op::Call: {
      NOT_YET();
      // RegOr<AnyFunc> r_fn;
      // if (cmd.call_.fn_.is_reg_) {
      //   auto iter = reg_relocs.find(cmd.call_.fn_.reg_);
      //   if (iter == reg_relocs.end()) { goto next_block; }
      //   r_fn = iter->second.get<AnyFunc>(0).reg_;
      // } else {
      //   r_fn = cmd.call_.fn_;
      // }

      // Results new_arg_results;
      // for (size_t i = 0; i < cmd.call_.arguments_->results().size(); ++i) {
      //   if (cmd.call_.arguments_->results().is_reg(i)) {
      //     auto iter =
      //         reg_relocs.find(cmd.call_.arguments_->results().get<Reg>(i));
      //     if (iter == reg_relocs.end()) { goto next_block; }
      //     new_arg_results.append(iter->second.GetResult(0));
      //   } else {
      //     new_arg_results.append(
      //         cmd.call_.arguments_->results().GetResult(i));
      //   }
      // }
      // Arguments new_args(cmd.call_.arguments_->type_,
      //                    std::move(new_arg_results));

      // if (cmd.call_.outs_) {
      //   OutParams outs;
      //   for (size_t i = 0; i < cmd.call_.outs_->regs_.size(); ++i) {
      //     if (cmd.call_.outs_->is_loc_[i]) {
      //       auto old_r = cmd.call_.outs_->regs_[i];
      //       auto iter  = reg_relocs.find(old_r);
      //       if (iter == reg_relocs.end()) { goto next_block; }
      //       // TODO reg_relocs.emplace(, op_fn(r0, r1));
      //     } else {
      //       auto r =
      //           Reserve(type::Int64);  // TODO this type is probably wrong.
      //       outs.is_loc_.push_back(false);
      //       outs.regs_.push_back(r);
      //       reg_relocs.emplace(cmd.call_.outs_->regs_[i], r);
      //     }
      //   }
      //   Call(r_fn, std::move(new_args), std::move(outs));
      // } else {
      //   Call(r_fn, std::move(new_args));
      // }
    } break;
    default:; NOT_YET(static_cast<int>(cmd.op_code_));
  }
}

#define CASES                                                                  \
  CASE(LegacyCmd);                                                             \
  CASE(PrintCmd);                                                              \
  CASE(AddCmd);                                                                \
  CASE(SubCmd);                                                                \
  CASE(MulCmd);                                                                \
  CASE(DivCmd);                                                                \
  CASE(ModCmd);                                                                \
  CASE(NegCmd);                                                                \
  CASE(NotCmd);                                                                \
  CASE(LtCmd);                                                                 \
  CASE(LeCmd);                                                                 \
  CASE(EqCmd);                                                                 \
  CASE(NeCmd);                                                                 \
  CASE(GeCmd);                                                                 \
  CASE(GtCmd);                                                                 \
  CASE(StoreCmd);                                                              \
  CASE(LoadCmd);                                                               \
  CASE(VariantCmd);                                                            \
  CASE(TupleCmd);                                                              \
  CASE(ArrowCmd);                                                              \
  CASE(PtrCmd);                                                                \
  CASE(BufPtrCmd);                                                             \
  CASE(JumpCmd);                                                               \
  CASE(XorFlagsCmd);                                                           \
  CASE(AndFlagsCmd);                                                           \
  CASE(OrFlagsCmd);                                                            \
  CASE(CastCmd);                                                               \
  CASE(RegisterCmd);                                                           \
  CASE(SetRetCmd);                                                             \
  CASE(EnumerationCmd);                                                        \
  CASE(StructCmd);                                                             \
  CASE(OpaqueTypeCmd);                                                         \
  CASE(SemanticCmd);                                                           \
  CASE(LoadSymbolCmd);                                                         \
  CASE(TypeInfoCmd);                                                           \
  CASE(AccessCmd);                                                             \
  CASE(VariantAccessCmd);                                                      \
  CASE(DebugIrCmd)

BlockIndex CmdBuffer::Execute(std::vector<ir::Addr> const& ret_slots,
                              backend::ExecContext* ctx) {
  auto iter = buf_.begin();
  DEBUG_LOG("dbg")(buf_);
  while (true) {
    DEBUG_LOG("dbg")(buf_.begin(), buf_.size());
    ASSERT(iter < buf_.end());
    switch (iter.read<cmd_index_t>()) {
#define CASE(type)                                                             \
  case type::index: {                                                          \
    DEBUG_LOG("dbg")(#type);                                                   \
    auto result = type::Execute(&iter, ret_slots, ctx);                        \
    if (result.has_value()) { return *result; }                                \
  } break
      CASES;
#undef CASE
      default: UNREACHABLE();
    }
  }
}

void CmdBuffer::UpdateForInlining(Inliner const& inliner) {
  auto iter = buf_.begin();
  DEBUG_LOG("dbg")(buf_);

  while (iter < buf_.end()) {
    switch (iter.read<cmd_index_t>()) {
#define CASE(type)                                                             \
  case type::index:                                                            \
    DEBUG_LOG("dbg")(#type ": ", iter);                                        \
    type::UpdateForInlining(&iter, inliner);                                   \
    break
      CASES;
#undef CASE
    }
  }
  
  DEBUG_LOG("dbg")(buf_);
}

std::string CmdBuffer::to_string() const {
  // Come up with a better/more-permanent solution here.
  std::string s;
  auto iter = buf_.begin();
  while (iter < buf_.end()) {
    switch (iter.read<cmd_index_t>()) {
#define CASE(type)                                                             \
  case type::index:                                                            \
    s.append("\n" #type ": ");                                                 \
    s.append(type::DebugString(&iter));                                        \
    break
      CASES;
#undef CASE
    }
  }
  return s;
}

#undef CASES

size_t GetOffset(CompiledFn const* fn, Reg r) {
  return fn->compiler_reg_to_offset_.at(r);
}
}  // namespace ir
