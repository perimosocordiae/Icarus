#ifndef ICARUS_IR_CMD_RETURN_H
#define ICARUS_IR_CMD_RETURN_H

#include "absl/strings/str_cat.h"
#include "ir/cmd/util.h"
#include "ir/cmd_buffer.h"

namespace ir {

struct ReturnCmd {
  constexpr static cmd_index_t index = 28;

  struct control_bits {
    uint8_t primitive_type : 6;
    uint8_t reg : 1;
    uint8_t only_get : 1;
  };

  template <typename T>
  static control_bits MakeControlBits(bool reg, bool only_get) {
    control_bits result;
    if (only_get) {
      result.primitive_type = 0;
      result.reg            = 0;
      result.only_get       = 1;
    } else {
      result.primitive_type = PrimitiveIndex<T>();
      result.reg            = reg;
      result.only_get       = 0;
    }
    return result;
  };

  static std::optional<BlockIndex> Execute(base::untyped_buffer::iterator* iter,
                                           std::vector<Addr> const& ret_slots,
                                           backend::ExecContext* ctx) {
    auto ctrl     = iter->read<control_bits>();
    uint16_t n    = iter->read<uint16_t>();
    Addr ret_slot = ret_slots[n];

    if (ctrl.only_get) {
      auto reg    = iter->read<Reg>();
      auto& frame = ctx->call_stack.top();
      frame.regs_.set(GetOffset(frame.fn_, reg), ret_slot);
      return std::nullopt;
    }
    DEBUG_LOG("return")("return slot #", n, " = ", ret_slot);

    ASSERT(ret_slot.kind == Addr::Kind::Heap);
    PrimitiveDispatch(ctrl.primitive_type, [&](auto tag) {
      using T = typename std::decay_t<decltype(tag)>::type;
      T val   = ctrl.reg ? ctx->resolve<T>(iter->read<Reg>()) : iter->read<T>();
      DEBUG_LOG("return")("val = ", val);
      *ASSERT_NOT_NULL(static_cast<T*>(ret_slot.as_heap)) = val;
    });
    return std::nullopt;
  }

  static std::string DebugString(base::untyped_buffer::const_iterator* iter) {
    using base::stringify;
    auto ctrl = iter->read<control_bits>();
    uint16_t n = iter->read<uint16_t>();

    if (ctrl.only_get) {
      return absl::StrCat(stringify(iter->read<Reg>()), " = get-ret ", n);
    }

    std::string s;
    if (ctrl.reg) {
      s = absl::StrCat(n, " ", stringify(iter->read<Reg>()));
    } else {
      // TODO: Add core::LayoutRequirements so you can skip forward by the
      // appropriate amount without instantiating so many templates.
      PrimitiveDispatch(ctrl.primitive_type, [&](auto tag) {
        s = absl::StrCat(
            n, " ",
            stringify(
                iter->read<typename std::decay_t<decltype(tag)>::type>()));
      });
    }
    return s;
  }

  static void UpdateForInlining(base::untyped_buffer::iterator* iter,
                                Inliner const& inliner) {
    auto ctrl = iter->read<control_bits>();
    iter->read<uint16_t>();

    if (ctrl.only_get) {
      inliner.Inline(&iter->read<Reg>());
      return;
    }

    if (ctrl.reg) {
      inliner.Inline(&iter->read<Reg>());
    } else {
      // TODO: Add core::LayoutRequirements so you can skip forward by the
      // appropriate amount without instantiating so many templates.
      PrimitiveDispatch(ctrl.primitive_type, [&](auto tag) {
        iter->read<typename std::decay_t<decltype(tag)>::type>();
      });
    }
  }
};

template <typename T>
void SetRet(uint16_t n, T val) {
  if constexpr (ir::IsRegOr<T>::value) {
    auto& blk = GetBuilder().function()->block(GetBuilder().CurrentBlock());
    blk.cmd_buffer_.append_index<ReturnCmd>();
    blk.cmd_buffer_.append(
        ReturnCmd::MakeControlBits<typename T::type>(val.is_reg_, false));
    blk.cmd_buffer_.append(n);
    if (val.is_reg_) {
      blk.cmd_buffer_.append(val.reg_);
    } else {
      blk.cmd_buffer_.append(val.val_);
    }
  } else if constexpr(IsTypedReg<T>::value) {
    SetRet(n, RegOr<typename T::type>(val));
  } else {
    SetRet(n, RegOr<T>(val));
  }
}

inline void SetRet(uint16_t n, type::Typed<Results> const& r, Context* ctx) {
  // if (r.type()->is<type::GenericStruct>()) {
  //   SetRet(n, r->get<AnyFunc>(0));
  // } else {
  type::Apply(r.type(), [&](auto tag) {
    using T = typename decltype(tag)::type;
    // if constexpr (std::is_same_v<T, type::Struct const*>) {
    //   auto* t = GetBuilder().function()->type_->output[n];
    //   // TODO guaranteed move-elision
    //   visitor::EmitIr visitor;
    //   t->EmitMoveAssign(&visitor, t, r.get(), GetRet(n, t), ctx);
    //   visitor.CompleteDeferredBodies();
    // } else {
    SetRet(n, r->get<T>(0));
    // }
  });
  // }
}

inline TypedRegister<Addr> GetRet(uint16_t n, type::Type const* t) {
  auto& blk = GetBuilder().function()->block(GetBuilder().CurrentBlock());
  blk.cmd_buffer_.append(ReturnCmd::MakeControlBits<int>(false, true));
  blk.cmd_buffer_.append(n);
  Reg r = MakeResult(t);
  blk.cmd_buffer_.append(r);
  return r;
}

}  // namespace ir

#endif  // ICARUS_IR_CMD_RETURN_H
