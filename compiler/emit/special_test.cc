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

using SpecialTest = testing::TestWithParam<TestCase>;
TEST_P(SpecialTest, Special) {
  auto const &[expr, expected] = GetParam();
  test::TestModule mod;
  // TODO: We can't use `s` as the field member because the compiler thinks
  // there's an ambiguity (there isn't).
  mod.AppendCode(R"(
  S ::= struct {
    n := 3
    p: *int64
  }
  )");
  auto const *e  = mod.Append<ast::Expression>(expr);
  auto const *qt = mod.context().qual_type(e);
  ASSERT_NE(qt, nullptr);
  auto t = qt->type();
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
INSTANTIATE_TEST_SUITE_P(All, SpecialTest,
                         testing::ValuesIn({
                             TestCase{.expr     = R"((() -> {
                               b: bool
                               return b
                             })()
                             )",
                                      .expected = ir::Value(false)},
                             TestCase{.expr     = R"((() -> {
                               n: nat8
                               return n
                             })()
                             )",
                                      .expected = ir::Value(uint8_t{0})},
                             TestCase{.expr     = R"((() -> {
                               n: nat16
                               return n
                             })()
                             )",
                                      .expected = ir::Value(uint16_t{0})},

                             TestCase{.expr     = R"((() -> {
                               n: nat32
                               return n
                             })()
                             )",
                                      .expected = ir::Value(uint32_t{0})},

                             TestCase{.expr     = R"((() -> {
                               n: nat64
                               return n
                             })()
                             )",
                                      .expected = ir::Value(uint64_t{0})},
                             TestCase{.expr     = R"((() -> {
                               n: int8
                               return n
                             })()
                             )",
                                      .expected = ir::Value(int8_t{0})},
                             TestCase{.expr     = R"((() -> {
                               n: int16
                               return n
                             })()
                             )",
                                      .expected = ir::Value(int16_t{0})},

                             TestCase{.expr     = R"((() -> {
                               n: int32
                               return n
                             })()
                             )",
                                      .expected = ir::Value(int32_t{0})},

                             TestCase{.expr     = R"((() -> {
                               n: int64
                               return n
                             })()
                             )",
                                      .expected = ir::Value(int64_t{0})},
                             TestCase{.expr     = R"((() -> {
                               p: *int64
                               return p
                             })()
                             )",
                                      .expected = ir::Value(ir::Addr::Null())},
                             TestCase{.expr     = R"((() -> {
                               p: [*]bool
                               return p
                             })()
                             )",
                                      .expected = ir::Value(ir::Addr::Null())},

                             TestCase{.expr     = R"((() -> {
                               // Test struct initialization
                               s: S
                               return s.p
                             })()
                             )",
                                      .expected = ir::Value(ir::Addr::Null())},

                             TestCase{.expr     = R"((() -> {
                               // Test struct initialization
                               s: S
                               return s.n
                             })()
                             )",
                                      .expected = ir::Value(int64_t{3})},

                             // TODO: Tests for tuples and arrays
                             // TODO: Tests for struct destructors, including
                             //       nested in arrays, tuples or other structs.
                             // TODO: Copy/move assignment tests

                         }));

}  // namespace
}  // namespace compiler