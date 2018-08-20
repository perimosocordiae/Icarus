#include "property/property_map.h"

#include "architecture.h"
#include "ir/func.h"
#include "property/property.h"
#include "type/function.h"

namespace debug {
extern bool validation;
}  // namespace debug

namespace prop {
FnStateView::FnStateView(IR::Func *fn) {
  for (const auto & [ num, reg ] : fn->reg_map_) {
    view_.emplace(reg, PropertySet{});
  }
}

void PropertyMap::AssumeReturnsTrue() {
  std::unordered_set<Entry> stale_up;
  for (const auto &block : fn_->blocks_) {
    for (const auto &cmd : block.cmds_) {
      if (cmd.op_code_ != IR::Op::SetReturnBool) { continue; }

      // TODO I actually know this on every block.
      lookup(&block, cmd.result).add(base::make_owned<prop::BoolProp>(true));
      stale_up.emplace(&block, cmd.result);
    }
  }

  refresh(std::move(stale_up), {});
}

PropertyMap::PropertyMap(IR::Func *fn) : fn_(fn) {
  // TODO copy fnstateview rather than creating it repeatedly?
  for (const auto &block : fn_->blocks_) {
    view_.emplace(&block, FnStateView(fn_));
  }

  for (auto const & [ f, pm ] : fn->preconditions_) {
    auto pm_copy = pm;
    pm_copy.AssumeReturnsTrue();

    // TODO all the registers
    this->lookup(&fn_->blocks_.at(0), IR::Register(0))
        .add(pm_copy.lookup(&f.blocks_.at(0), IR::Register(0)));
  }

  // TODO all the argument registers.
  std::unordered_set<Entry> stale_down;
  for (auto const &block : fn->blocks_) {
    stale_down.emplace(&block, IR::Register(0));
  }

  // This refresh is an optimization. Because it's likely that this gets called
  // many times with different arguments, it's better to precompute whatever can
  // be on this function rather than repeating all that on many different calls.
  refresh({}, std::move(stale_down));
}

namespace {
template <typename SetContainer, typename Fn>
void until_empty(SetContainer *container, Fn &&fn) {
  while (!container->empty()) {
    auto iter = container->begin();
    while (iter != container->end()) {
      auto next_iter = std::next(iter);
      fn(container->extract(iter).value());
      iter = next_iter;
    }
  }
}

PropertySet Not(PropertySet prop_set) {
  prop_set.props_.for_each([](base::owned_ptr<Property> *prop) {
    if (!(**prop).is<BoolProp>()) { return; }
    auto p           = &(**prop).as<BoolProp>();
    p->can_be_true_  = !p->can_be_true_;
    p->can_be_false_ = !p->can_be_false_;
  });
  return prop_set;
}

PropertySet NeBool(PropertySet const &lhs, PropertySet const &rhs) {
  return {};  // TODO
}

PropertySet EqBool(PropertySet const &lhs, PropertySet const &rhs) {
  return {};  // TODO
}

PropertySet LtInt(PropertySet const &lhs, int rhs) {
  auto b = base::make_owned<BoolProp>();
  lhs.props_.for_each([&b, rhs](base::owned_ptr<Property> const *prop) {
    if (!(**prop).is<IntProp>()) { return; }
    auto p = &(**prop).as<IntProp>();
    if (p->lower_) {
      if (p->bound_ >= rhs) { *b = BoolProp(false); }
    } else {
      if (p->bound_ <= rhs) { *b = BoolProp(true); }
    }
  });
  PropertySet ps;
  ps.add(b);
  return ps;
}

}  // namespace

// TODO no longer need to pass stale in as ptr.
void PropertyMap::MarkReferencesStale(Entry const &e,
                                      std::unordered_set<Entry> *stale_down) {
  for (IR::Register reg : fn_->references_.at(e.reg_)) {
    stale_down->emplace(e.viewing_block_, reg);
  }

  auto &last_cmd = e.viewing_block_->cmds_.back();
  switch (last_cmd.op_code_) {
    case IR::Op::UncondJump:
      stale_down->emplace(&fn_->block(last_cmd.uncond_jump_.block_), e.reg_);
      break;
    case IR::Op::CondJump:
      stale_down->emplace(&fn_->block(last_cmd.cond_jump_.blocks_[0]), e.reg_);
      stale_down->emplace(&fn_->block(last_cmd.cond_jump_.blocks_[1]), e.reg_);
      break;
    case IR::Op::ReturnJump: break;
    default: UNREACHABLE();
  }
}

static void Debug(PropertyMap const &pm) {
  fprintf(stderr, "\033[2J\033[1;1H\n");  // Clear the screen
  LOG << pm.fn_;
  for (auto const & [ block, view ] : pm.view_) {
    fprintf(stderr, "VIEWING BLOCK: %lu\n", reinterpret_cast<uintptr_t>(block));
    for (auto const & [ reg, prop_set ] : view.view_) {
      fprintf(stderr, "  reg.%d:\n", reg.value);
      for (auto const &p : prop_set.props_) {
        fprintf(stderr, "    %s\n", p->to_string().c_str());
      }
    }
    fprintf(stderr, "\n");
  }
  fgetc(stdin);
}

bool PropertyMap::UpdateEntryFromAbove(Entry const &e) {
  if (debug::validation) { Debug(*this); }

  auto *cmd_ptr = fn_->Command(e.reg_);
  if (cmd_ptr == nullptr) {
    auto inc  = fn_->GetIncomingBlocks();
    auto iter = inc.find(e.viewing_block_);
    if (iter != inc.end()) {
      auto &prop_set = this->lookup(e);
      bool changed   = false;
      for (auto const *incoming_block : iter->second) {
        changed |= prop_set.add(this->lookup(incoming_block, e.reg_));
      }
    }
    return true;
  }

  auto &cmd        = *cmd_ptr;
  auto &block_view = view_.at(e.viewing_block_).view_;
  auto &prop_set   = block_view.at(e.reg_);

  switch (cmd.op_code_) {
    case IR::Op::UncondJump: return /* TODO */ false;
    case IR::Op::CondJump: return /* TODO */ false;
    case IR::Op::ReturnJump: return /* TODO */ false;
    case IR::Op::Call: return /* TODO */ false;
    case IR::Op::Not: return prop_set.add(Not(block_view.at(cmd.not_.reg_)));
    case IR::Op::EqBool:
      return prop_set.add(EqBool(block_view.at(cmd.eq_bool_.args_[0]),
                                 block_view.at(cmd.eq_bool_.args_[1])));
    case IR::Op::NeBool:
      return prop_set.add(NeBool(block_view.at(cmd.ne_bool_.args_[0]),
                                 block_view.at(cmd.ne_bool_.args_[1])));
    case IR::Op::LtInt:
      if (cmd.lt_int_.args_[0].is_reg_) {
        if (cmd.lt_int_.args_[1].is_reg_) {
          NOT_YET();
        } else {
          return prop_set.add(LtInt(block_view.at(cmd.lt_int_.args_[0].reg_),
                                    cmd.lt_int_.args_[1].val_));
        }
      } else {
        NOT_YET();
      }

    case IR::Op::SetReturnBool:
      if (cmd.set_return_bool_.val_.is_reg_) {
        prop_set.add(block_view.at(cmd.set_return_bool_.val_.reg_));
        // TODO Do I need to mark stale upwards?
      } else {
        prop_set.add(
            base::make_owned<BoolProp>(cmd.set_return_bool_.val_.val_));
      }
      return false;
    default: NOT_YET(cmd.op_code_);
  }
}

void PropertyMap::UpdateEntryFromBelow(Entry const &e,
                                       std::unordered_set<Entry> *stale_up,
                                       std::unordered_set<Entry> *stale_down) {
  if (debug::validation) { Debug(*this); }

  auto *cmd_ptr = fn_->Command(e.reg_);
  if (cmd_ptr == nullptr) {
    stale_down->insert(e);
    return;
  }
  auto &cmd  = *ASSERT_NOT_NULL(cmd_ptr);
  auto &view = view_.at(e.viewing_block_).view_;
  switch (cmd.op_code_) {
#define DEFINE_CASE(op_name)                                                   \
  {                                                                            \
    if (cmd.op_name.val_.is_reg_) {                                            \
      view.at(cmd.op_name.val_.reg_).add(view.at(e.reg_));                     \
      stale_up->emplace(e.viewing_block_, cmd.op_name.val_.reg_);              \
    }                                                                          \
  }                                                                            \
  break
    case IR::Op::SetReturnBool: DEFINE_CASE(set_return_bool_);
    case IR::Op::SetReturnChar: DEFINE_CASE(set_return_char_);
    case IR::Op::SetReturnInt: DEFINE_CASE(set_return_int_);
    case IR::Op::SetReturnReal: DEFINE_CASE(set_return_real_);
    case IR::Op::SetReturnType: DEFINE_CASE(set_return_type_);
    case IR::Op::SetReturnEnum: DEFINE_CASE(set_return_enum_);
    case IR::Op::SetReturnCharBuf: DEFINE_CASE(set_return_char_buf_);
    case IR::Op::SetReturnFlags: DEFINE_CASE(set_return_flags_);
    case IR::Op::SetReturnAddr: DEFINE_CASE(set_return_addr_);
    case IR::Op::SetReturnFunc: DEFINE_CASE(set_return_func_);
    case IR::Op::SetReturnScope: DEFINE_CASE(set_return_scope_);
    case IR::Op::SetReturnModule: DEFINE_CASE(set_return_module_);
    case IR::Op::SetReturnGeneric: DEFINE_CASE(set_return_generic_);
    case IR::Op::SetReturnBlock: DEFINE_CASE(set_return_block_);
#undef DEFINE_CASE
    case IR::Op::Not: {
      bool changed = view.at(cmd.not_.reg_).add(Not(view.at(e.reg_)));
      if (changed) { stale_up->emplace(e.viewing_block_, cmd.not_.reg_); }
    } break;
    default: NOT_YET(cmd);
  }
}

void PropertyMap::refresh(std::unordered_set<Entry> stale_up,
                          std::unordered_set<Entry> stale_down) {
  do {
    until_empty(&stale_down, [this, &stale_down](Entry const &e) {
      if (this->UpdateEntryFromAbove(e)) {
        MarkReferencesStale(e, &stale_down);
      }
    });
    until_empty(&stale_up, [this, &stale_up, &stale_down](Entry const &e) {
      this->UpdateEntryFromBelow(e, &stale_up, &stale_down);
    });
  } while (!stale_down.empty());
}

// TODO this is not a great way to handle this. Probably should store all
// set-rets first.
BoolProp PropertyMap::Returns() const {
  base::vector<IR::CmdIndex> rets;
  base::vector<IR::Register> regs;

  // This can be precompeted and stored on the actual IR::Func.
  i32 num_blocks = static_cast<i32>(fn_->blocks_.size());
  for (i32 i = 0; i < num_blocks; ++i) {
    const auto &block = fn_->blocks_[i];
    i32 num_cmds      = static_cast<i32>(block.cmds_.size());
    for (i32 j = 0; j < num_cmds; ++j) {
      const auto &cmd = block.cmds_[j];
      if (cmd.op_code_ == IR::Op::SetReturnBool) {
        rets.push_back(IR::CmdIndex{IR::BlockIndex{i}, j});
        regs.push_back(cmd.result);
      }
    }
  }

  auto bool_ret = BoolProp::Bottom();

  for (size_t i = 0; i < rets.size(); ++i) {
    IR::BasicBlock *block = &fn_->blocks_[rets.at(i).block.value];
    BoolProp acc;
    lookup(block, regs.at(i)).accumulate(&acc);
    bool_ret |= acc;
  }

  return bool_ret;
}

PropertyMap PropertyMap::with_args(IR::LongArgs const &args,
                                   FnStateView const &fn_state_view) const {
  auto copy         = *this;
  auto *entry_block = &fn_->block(fn_->entry());
  auto &props       = copy.view_.at(entry_block).view_;

  auto arch     = Architecture::InterprettingMachine();
  size_t offset = 0;
  size_t index  = 0;
  // TODO offset < args.args_.size() should work as the condition but it isn't,
  // so figure out why.

  std::unordered_set<Entry> stale_down;
  while (index < args.type_->input.size()) {
    auto *t = args.type_->input.at(index);
    offset  = arch.MoveForwardToAlignment(t, offset);
    // TODO instead of looking for non-register args, this should be moved out
    // to the caller. because registers might also have some properties that can
    // be reasoned about, all of this should be figured out where it's known and
    // then passed in.
    if (args.is_reg_.at(index)) {
      props.at(IR::Register(offset))
          .add(fn_state_view.view_.at(args.args_.get<IR::Register>(offset)));

      // TODO only need to do this on the entry block, but we're not passing
      // info between block views yet.
      for (const auto &b : fn_->blocks_) {
        stale_down.emplace(&b, IR::Register(offset));
      }
      offset += sizeof(IR::Register);
    } else {
      if (t == type::Bool) {
        props.at(IR::Register(offset))
            .add(base::make_owned<BoolProp>(args.args_.get<bool>(offset)));
        // TODO only need to do this on the entry block, but we're not passing
        // info between block views yet.
        for (const auto &b : fn_->blocks_) {
          stale_down.emplace(&b, IR::Register(offset));
        }
      }
      offset += arch.bytes(t);
    }
    ++index;
  }

  copy.refresh({}, std::move(stale_down));
  return copy;
}

}  // namespace prop
