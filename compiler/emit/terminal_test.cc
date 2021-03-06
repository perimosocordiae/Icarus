#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/module.h"
#include "type/primitive.h"

namespace compiler {
namespace {

struct TestCase {
  std::string terminal;
  ir::Value expected;
};

using TerminalTest = testing::TestWithParam<TestCase>;
TEST_P(TerminalTest, Terminal) {
  auto const &[terminal, expected] = GetParam();
  test::TestModule mod;
  auto const *term = mod.Append<ast::Terminal>(terminal);
  auto result      = mod.compiler.Evaluate(type::Typed<ast::Expression const *>(
      term, mod.context().qual_types(term)[0].type()));
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, expected);
}

INSTANTIATE_TEST_SUITE_P(
    All, TerminalTest,
    testing::ValuesIn({
        TestCase{.terminal = "true", .expected = ir::Value(true)},
        TestCase{.terminal = "false", .expected = ir::Value(false)},
        TestCase{.terminal = "null", .expected = ir::Value(ir::Null())},
        TestCase{.terminal = "u8", .expected = ir::Value(type::U8)},
        TestCase{.terminal = "u16", .expected = ir::Value(type::U16)},
        TestCase{.terminal = "u32", .expected = ir::Value(type::U32)},
        TestCase{.terminal = "u64", .expected = ir::Value(type::U64)},
        TestCase{.terminal = "i8", .expected = ir::Value(type::I8)},
        TestCase{.terminal = "i16", .expected = ir::Value(type::I16)},
        TestCase{.terminal = "i32", .expected = ir::Value(type::I32)},
        TestCase{.terminal = "i64", .expected = ir::Value(type::I64)},
        TestCase{.terminal = "f32", .expected = ir::Value(type::F32)},
        TestCase{.terminal = "f64", .expected = ir::Value(type::F64)},
        TestCase{.terminal = "bool", .expected = ir::Value(type::Bool)},
        TestCase{.terminal = "type", .expected = ir::Value(type::Type_)},
        TestCase{.terminal = "module", .expected = ir::Value(type::Module)},

        // TODO: Evaluating string literals. Requires looking at read-only
        // addresses.
        // TODO: Integers and their edge cases. Especially INT_MIN.
        // TODO: Floating point edge cases.
        // TODO: Determine how you will be allowed to specify arithmetic
        // literals other than i64 or f64.
    }));

}  // namespace
}  // namespace compiler
