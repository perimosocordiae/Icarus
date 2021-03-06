#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/module.h"
#include "type/primitive.h"

namespace compiler {
namespace {

struct TestCase {
  std::string context;
  std::string expr;
  ir::Value expected;
};

using UnaryOperatorTest = testing::TestWithParam<TestCase>;
TEST_P(UnaryOperatorTest, UnaryOperator) {
  auto const &[context, expr, expected] = GetParam();
  test::TestModule mod;
  mod.AppendCode(context);
  auto const *e  = mod.Append<ast::Expression>(expr);
  auto t         = mod.context().qual_types(e)[0].type();
  ASSERT_TRUE(t.valid());
  auto result =
      mod.compiler.Evaluate(type::Typed<ast::Expression const *>(e, t));
  ASSERT_TRUE(result);
  EXPECT_EQ(*result, expected);
}

// Note: We test both with literals and with a unary-operator applied directly
// to a function call. The former helps cover the constant-folding mechanisms
// built in to the ir::Builder. The latter helps cover the common case for code
// emission.
INSTANTIATE_TEST_SUITE_P(
    All, UnaryOperatorTest,
    testing::ValuesIn({
        TestCase{.expr = "move 3", .expected = ir::Value(int64_t{3})},
        TestCase{.context  = "f ::= () => 3",
                 .expr     = "move f()",
                 .expected = ir::Value(int64_t{3})},
        TestCase{.expr = "copy 3", .expected = ir::Value(int64_t{3})},
        // TODO: Test move/copy for non-trivial types.
        TestCase{.context  = "f ::= () => 3",
                 .expr     = "copy f()",
                 .expected = ir::Value(int64_t{3})},
        TestCase{.expr     = "[*]i32",
                 .expected = ir::Value(
                     static_cast<type::Type>(type::BufPtr(type::I32)))},
        TestCase{.context  = "f ::= () => bool",
                 .expr     = "[*]f()",
                 .expected = ir::Value(
                     static_cast<type::Type>(type::BufPtr(type::Bool)))},
        TestCase{.expr = "not true", .expected = ir::Value(false)},
        TestCase{.context  = "f ::= () => false",
                 .expr     = "not f()",
                 .expected = ir::Value(true)},
        // TODO: Test flag negation. The problem is is we randomize the flag
        // values so we need a way to extract specific values. Perhaps this is
        // better done outside this parameterized test.
        TestCase{.expr = "-(3 as i8)", .expected = ir::Value(int8_t{-3})},
        TestCase{.context  = "f ::= () => 3 as i8",
                 .expr     = "-f()",
                 .expected = ir::Value(int8_t{-3})},
        TestCase{.expr = "-(3 as i16)", .expected = ir::Value(int16_t{-3})},
        TestCase{.context  = "f ::= () => 3 as i16",
                 .expr     = "-f()",
                 .expected = ir::Value(int16_t{-3})},
        TestCase{.expr = "-(3 as i32)", .expected = ir::Value(int32_t{-3})},
        TestCase{.context  = "f ::= () => 3 as i32",
                 .expr     = "-f()",
                 .expected = ir::Value(int32_t{-3})},
        TestCase{.expr = "-(3 as i64)", .expected = ir::Value(int64_t{-3})},
        TestCase{.context  = "f ::= () => 3 as i64",
                 .expr     = "-f()",
                 .expected = ir::Value(int64_t{-3})},
        TestCase{.expr = "-(3.0 as f32)", .expected = ir::Value(float{-3})},
        TestCase{.context  = "f ::= () => 3 as f32",
                 .expr     = "-f()",
                 .expected = ir::Value(float{-3})},
        TestCase{.expr = "-(3.0 as f32)", .expected = ir::Value(float{-3})},
        TestCase{.context  = "f ::= () => 3 as f32",
                 .expr     = "-f()",
                 .expected = ir::Value(float{-3})},
        TestCase{.expr = "-(3.0 as f64)", .expected = ir::Value(double{-3})},
        TestCase{.context  = "f ::= () => 3 as f64",
                 .expr     = "-f()",
                 .expected = ir::Value(double{-3})},
        TestCase{.expr     = "true:?",
                 .expected = ir::Value(static_cast<type::Type>(type::Bool))},
        TestCase{.context  = "f ::= () => true",
                 .expr     = "f():?",
                 .expected = ir::Value(static_cast<type::Type>(type::Bool))},
        TestCase{.expr = "*i32",
                 .expected =
                     ir::Value(static_cast<type::Type>(type::Ptr(type::I32)))},
        TestCase{.context = "f ::= () => i32",
                 .expr    = "*f()",
                 .expected =
                     ir::Value(static_cast<type::Type>(type::Ptr(type::I32)))},
        TestCase{.context  = R"(
               f ::= () -> i64 {
                 n: i64
                 np := &n
                 n = 3
                 return @np
               }
               )",
                 .expr     = "f()",
                 .expected = ir::Value(int64_t{3})},
        TestCase{.context  = R"(*[*]i64 ~ *`T)",
                 .expr     = "T",
                 .expected = ir::Value(type::Type(type::BufPtr(type::I64)))},
        TestCase{.context  = R"([*]*i64 ~ [*]`T)",
                 .expr     = "T",
                 .expected = ir::Value(type::Type(type::Ptr(type::I64)))},
        TestCase{.context  = R"(3 ~ -`N)",
                 .expr     = "N",
                 .expected = ir::Value(int64_t{-3})},
        TestCase{.context  = R"(3.1 ~ -`N)",
                 .expr     = "N",
                 .expected = ir::Value(-3.1)},
        TestCase{.context  = R"(true ~ not `B)",
                 .expr     = "B",
                 .expected = ir::Value(false)},
    }));

}  // namespace
}  // namespace compiler
