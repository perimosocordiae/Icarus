#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/module.h"
#include "type/primitive.h"

namespace compiler {
namespace {

struct TestCase {
  std::string expr;
  ir::Value expected;
};

using ComparisonOperatorTest = testing::TestWithParam<TestCase>;
TEST_P(ComparisonOperatorTest, Struct) {
  auto const &[expr, expected] = GetParam();
  test::TestModule mod;
  // TODO: We can't use `s` as the field member because the compiler thinks
  // there's an ambiguity (there isn't).
  mod.AppendCode(R"(
  S ::= struct {
    n: i64
    p: *i64
    sp: *S
  }
  )");
  auto const *e = mod.Append<ast::Expression>(expr);
  auto t        = mod.context().qual_types(e)[0].type();
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
    All, ComparisonOperatorTest,
    testing::ValuesIn({
        TestCase{.expr     = R"(((x: i64, y: i64) => x < y)(1, 2))",
                 .expected = ir::Value(true)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x < y)(1, 1))",
                 .expected = ir::Value(false)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x < y)(2, 1))",
                 .expected = ir::Value(false)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x <= y)(1, 2))",
                 .expected = ir::Value(true)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x <= y)(1, 1))",
                 .expected = ir::Value(true)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x <= y)(2, 1))",
                 .expected = ir::Value(false)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x == y)(1, 2))",
                 .expected = ir::Value(false)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x == y)(1, 1))",
                 .expected = ir::Value(true)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x != y)(2, 1))",
                 .expected = ir::Value(true)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x != y)(1, 1))",
                 .expected = ir::Value(false)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x > y)(1, 2))",
                 .expected = ir::Value(false)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x > y)(1, 1))",
                 .expected = ir::Value(false)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x > y)(2, 1))",
                 .expected = ir::Value(true)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x >= y)(1, 2))",
                 .expected = ir::Value(false)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x >= y)(1, 1))",
                 .expected = ir::Value(true)},
        TestCase{.expr     = R"(((x: i64, y: i64) => x >= y)(2, 1))",
                 .expected = ir::Value(true)},

        TestCase{
            .expr = R"(((x: i64, y: i64, z: i64) => x < y < z)(1, 2, 3))",
            .expected = ir::Value(true)},
        TestCase{
            .expr = R"(((x: i64, y: i64, z: i64) => x < y < z)(1, 2, 1))",
            .expected = ir::Value(false)},
        TestCase{
            .expr =
                R"(((x: i64, y: i64, z: i64) => x < y <= z)(1, 2, 2))",
            .expected = ir::Value(true)},
        TestCase{
            .expr = R"(((x: i64, y: i64, z: i64) => x < y > z)(2, 3, 1))",
            .expected = ir::Value(true)},
        TestCase{
            .expr = R"(((x: i64, y: i64, z: i64) => x < y > z)(1, 2, 1))",
            .expected = ir::Value(true)},
        TestCase{
            .expr = R"(((x: i64, y: i64, z: i64) => x < y > z)(2, 3, 4))",
            .expected = ir::Value(false)},
    }));

// TODO: Add a test that covers buffer pointer comparisons.

}  // namespace
}  // namespace compiler
