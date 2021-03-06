#include "ir/blocks/register_allocator.h"
#include "type/primitive.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace ir {
namespace {
using testing::_;
using testing::MockFunction;
using testing::Return;

TEST(RegisterAllocator, NumRegs) {
  RegisterAllocator a(3);
  EXPECT_EQ(a.num_regs(), 3);
  EXPECT_EQ(a.num_args(), 3);

  EXPECT_EQ(a.Reserve(), Reg{3});
  EXPECT_EQ(a.Reserve(), Reg{4});
  EXPECT_EQ(a.num_regs(), 5);
  EXPECT_EQ(a.num_args(), 3);

  EXPECT_EQ(a.StackAllocate(type::I32), Reg{5});
  EXPECT_EQ(a.StackAllocate(type::I32), Reg{6});
  EXPECT_EQ(a.num_regs(), 7);
  EXPECT_EQ(a.num_args(), 3);
}

TEST(RegisterAllocator, ForEachAlloc) {
  RegisterAllocator a(3);
  a.StackAllocate(type::I32);
  a.StackAllocate(type::I64);

  MockFunction<void(type::Type, Reg)> mock_fn;
  EXPECT_CALL(mock_fn, Call(type::I32, _)).Times(1);
  EXPECT_CALL(mock_fn, Call(type::I64, _)).Times(1);
  a.for_each_alloc(mock_fn.AsStdFunction());
}

TEST(RegisterAllocator, MergeFrom) {
  RegisterAllocator a1(3);
  RegisterAllocator a2(4);
  a1.StackAllocate(type::I32);
  a2.StackAllocate(type::I64);
  a2.StackAllocate(type::Bool);

  MockFunction<Reg(Reg)> mock_fn;
  EXPECT_CALL(mock_fn, Call(Reg{4})).WillOnce(Return(Reg{10}));
  EXPECT_CALL(mock_fn, Call(Reg{5})).WillOnce(Return(Reg{11}));

  a1.MergeFrom(a2, mock_fn.AsStdFunction());
  EXPECT_EQ(a1.num_regs(), 10);
}

}  // namespace
}  // namespace ir
