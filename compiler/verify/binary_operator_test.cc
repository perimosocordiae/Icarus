#include "absl/strings/str_format.h"
#include "compiler/compiler.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/module.h"
#include "type/primitive.h"

namespace compiler {
namespace {

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

using FlagsLogicalOperatorEq = testing::TestWithParam<char const *>;
TEST_P(FlagsLogicalOperatorEq, Success) {
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(R"(
    F ::= flags { A \\ B \\ C }
    f: F
    f %s f
    )",
                                 GetParam()));
  EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
}

TEST_P(FlagsLogicalOperatorEq, NonReference) {
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(R"(
    F ::= flags { A \\ B \\ C }
    f: F
    not f %s f
    )",
                                 GetParam()));
  EXPECT_THAT(
      mod.consumer.diagnostics(),
      UnorderedElementsAre(Pair("value-category-error",
                                "invalid-assignment-lhs-value-category")));
}

TEST_P(FlagsLogicalOperatorEq, Constant) {
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(R"(
    F ::= flags { A \\ B \\ C }
    f :: F
    f %s F.A
    )",
                                 GetParam()));
  EXPECT_THAT(
      mod.consumer.diagnostics(),
      UnorderedElementsAre(Pair("value-category-error",
                                "invalid-assignment-lhs-value-category")));
}

TEST_P(FlagsLogicalOperatorEq, InvalidLhsType) {
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(R"(
    F ::= flags { A \\ B \\ C }
    n: i64
    n %s F.A
    )",
                                 GetParam()));

  EXPECT_THAT(mod.consumer.diagnostics(),
              UnorderedElementsAre(Pair(
                  "type-error", "logical-assignment-needs-bool-or-flags")));
}

TEST_P(FlagsLogicalOperatorEq, InvalidRhsType) {
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(R"(
    F ::= flags { A \\ B \\ C }
    f: F
    f %s 3
    )",
                                 GetParam()));

  EXPECT_THAT(mod.consumer.diagnostics(),
              UnorderedElementsAre(Pair(
                  "type-error", "logical-assignment-needs-bool-or-flags")));
}
INSTANTIATE_TEST_SUITE_P(All, FlagsLogicalOperatorEq,
                         testing::ValuesIn({"^=", "&=", "|="}));

using IntegerArithmeticOperatorEq =
    testing::TestWithParam<std::tuple<char const *, char const *>>;
TEST_P(IntegerArithmeticOperatorEq, Success) {
  auto [type, op] = GetParam();
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(
      R"(n: %s
         n %s n
      )",
      type, op));
  EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
}

TEST_P(IntegerArithmeticOperatorEq, NonReference) {
  auto [type, op] = GetParam();
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(
      R"(n: %s
         // `n + n` is a valid non-reference expression for all integral types.
         (n + n) %s n
      )",
      type, op));
  EXPECT_THAT(
      mod.consumer.diagnostics(),
      UnorderedElementsAre(Pair("value-category-error",
                                "invalid-assignment-lhs-value-category")));
}

TEST_P(IntegerArithmeticOperatorEq, Constant) {
  auto [type, op] = GetParam();
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(
      R"(n :: %s
         n %s n
      )",
      type, op));
  EXPECT_THAT(
      mod.consumer.diagnostics(),
      UnorderedElementsAre(Pair("value-category-error",
                                "invalid-assignment-lhs-value-category")));
}

TEST_P(IntegerArithmeticOperatorEq, InvalidLhsType) {
  auto [type, op] = GetParam();
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(
      R"(n: %s
         b: bool
         b %s n
      )",
      type, op));

  EXPECT_THAT(
      mod.consumer.diagnostics(),
      UnorderedElementsAre(Pair("type-error", "no-matching-binary-operator")));
}

TEST_P(IntegerArithmeticOperatorEq, InvalidRhsType) {
  auto [type, op] = GetParam();
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(
      R"(n: %s
         n %s true
      )",
      type, op));

  EXPECT_THAT(
      mod.consumer.diagnostics(),
      UnorderedElementsAre(Pair("type-error", "no-matching-binary-operator")));
}
INSTANTIATE_TEST_SUITE_P(
    All, IntegerArithmeticOperatorEq,
    testing::Combine(testing::ValuesIn({"i8", "i16", "i32", "i64", "u8", "u16",
                                        "u32", "u64"}),
                     testing::ValuesIn({"+=", "-=", "*=", "/=", "%="})));

using FloatingPointArithmeticOperatorEq =
    testing::TestWithParam<std::tuple<char const *, char const *>>;
TEST_P(FloatingPointArithmeticOperatorEq, Success) {
  auto [type, op] = GetParam();
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(
      R"(n: %s
         n %s n
      )",
      type, op));
  EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
}

TEST_P(FloatingPointArithmeticOperatorEq, NonReference) {
  auto [type, op] = GetParam();
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(
      R"(x: %s
         // `x + x` is a valid non-reference expression for all floating-point types.
         (x + x) %s x
      )",
      type, op));
  EXPECT_THAT(
      mod.consumer.diagnostics(),
      UnorderedElementsAre(Pair("value-category-error",
                                "invalid-assignment-lhs-value-category")));
}

TEST_P(FloatingPointArithmeticOperatorEq, Constant) {
  auto [type, op] = GetParam();
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(
      R"(x :: %s
         x %s x
      )",
      type, op));
  EXPECT_THAT(
      mod.consumer.diagnostics(),
      UnorderedElementsAre(Pair("value-category-error",
                                "invalid-assignment-lhs-value-category")));
}

TEST_P(FloatingPointArithmeticOperatorEq, InvalidLhsType) {
  auto [type, op] = GetParam();
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(
      R"(x: %s
         b: bool
         b %s x
      )",
      type, op));

  EXPECT_THAT(
      mod.consumer.diagnostics(),
      UnorderedElementsAre(Pair("type-error", "no-matching-binary-operator")));
}

TEST_P(FloatingPointArithmeticOperatorEq, InvalidRhsType) {
  auto [type, op] = GetParam();
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(
      R"(x: %s
         x %s true
      )",
      type, op));

  EXPECT_THAT(
      mod.consumer.diagnostics(),
      UnorderedElementsAre(Pair("type-error", "no-matching-binary-operator")));
}
INSTANTIATE_TEST_SUITE_P(
    All, FloatingPointArithmeticOperatorEq,
    testing::Combine(testing::ValuesIn({"f32", "f64"}),
                     testing::ValuesIn({"+=", "-=", "*=", "/="})));

using BinaryOperator =
    testing::TestWithParam<std::tuple<char const *, char const *>>;
TEST_P(BinaryOperator, Success) {
  auto [type, op] = GetParam();
  {
    test::TestModule mod;
    mod.AppendCode("FlagType ::= flags {}");
    mod.AppendCode(absl::StrCat(R"(x: )", type));
    auto const *expr =
        mod.Append<ast::BinaryOperator>(absl::StrFormat("x %s x", op));
    auto qts = mod.context().qual_types(expr);
    EXPECT_EQ(qts[0].quals(), type::Quals::Unqualified());
    EXPECT_EQ(qts[0].type(), mod.context().qual_types(expr->lhs())[0].type());
    EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
  }
  {
    test::TestModule mod;
    mod.AppendCode("FlagType ::= flags {}");
    mod.AppendCode(absl::StrCat(R"(x :: )", type));
    auto const *expr =
        mod.Append<ast::BinaryOperator>(absl::StrFormat("x %s x", op));
    auto qts = mod.context().qual_types(expr);
    EXPECT_EQ(qts[0].quals(), type::Quals::Const());
    EXPECT_EQ(qts[0].type(), mod.context().qual_types(expr->lhs())[0].type());
    EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
  }
}

INSTANTIATE_TEST_SUITE_P(
    Integers, BinaryOperator,
    testing::Combine(testing::ValuesIn({"i8", "i16", "i32", "i64", "u8", "u16",
                                        "u32", "u64"}),
                     testing::ValuesIn({"+", "-", "*", "/", "%"})));
INSTANTIATE_TEST_SUITE_P(FloatingPoint, BinaryOperator,
                         testing::Combine(testing::ValuesIn({"f32", "f64"}),
                                          testing::ValuesIn({"+", "-", "*",
                                                             "/"})));
INSTANTIATE_TEST_SUITE_P(
    FlagsOperator, BinaryOperator,
    testing::ValuesIn({std::tuple<char const *, char const *>("FlagType", "&"),
                       std::tuple<char const *, char const *>("FlagType", "|"),
                       std::tuple<char const *, char const *>("FlagType",
                                                              "^")}));

INSTANTIATE_TEST_SUITE_P(
    LogicalOperator, BinaryOperator,
    testing::ValuesIn({std::tuple<char const *, char const *>("bool", "and"),
                       std::tuple<char const *, char const *>("bool", "or"),
                       std::tuple<char const *, char const *>("bool", "xor")}));

using OperatorOverload = testing::TestWithParam<char const *>;
TEST_P(OperatorOverload, Overloads) {
  test::TestModule mod;
  mod.AppendCode(absl::StrFormat(
      R"(S ::= struct {}
         (%s) ::= (lhs: S, rhs: S) -> i64 { return 0 }
      )",
      GetParam()));
  auto const *expr = mod.Append<ast::BinaryOperator>(
      absl::StrFormat("S.{} %s S.{}", GetParam()));
  auto qts = mod.context().qual_types(expr);
  EXPECT_THAT(qts,
              UnorderedElementsAre(type::QualType::NonConstant(type::I64)));
  EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
}

TEST_P(OperatorOverload, MissingOverloads) {
  test::TestModule mod;
  mod.AppendCode(
      R"(S ::= struct {}
      )");
  auto const *expr = mod.Append<ast::BinaryOperator>(
      absl::StrFormat("S.{} %s S.{}", GetParam()));
  auto qts = mod.context().qual_types(expr);
  EXPECT_THAT(qts, UnorderedElementsAre(type::QualType::Error()));
  EXPECT_THAT(mod.consumer.diagnostics(),
              UnorderedElementsAre(
                  Pair("type-error", "invalid-binary-operator-overload")));
}

INSTANTIATE_TEST_SUITE_P(All, OperatorOverload,
                         testing::ValuesIn({"+", "-", "*", "/", "%", "^", "&",
                                            "|"}));

TEST(BufferPointerArithmetic, Success) {
  test::TestModule mod;
  mod.AppendCode(R"(
    p: [*]i64
    p += 1 as u8
    p += 1
    p -= 1
    p -= 1 as u32
    p = p - 1
    p = p + 1
    p = 1 + p
    n: i64 = p - p
  )");
  EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
}

TEST(BufferPointerArithmetic, Failure) {
  test::TestModule mod;
  mod.AppendCode(R"(
    p: [*]i64
    q: [*]u64
    p + p
    p += 1.2
    p = 1 - p
    q - p
  )");
  EXPECT_THAT(mod.consumer.diagnostics(),
              UnorderedElementsAre(
                  Pair("type-error", "no-matching-binary-operator"),
                  Pair("type-error", "no-matching-binary-operator"),
                  Pair("type-error", "no-matching-binary-operator"),
                  Pair("type-error", "no-matching-binary-operator")));
}

TEST(Unexpanded, Failure) {
  test::TestModule mod;
  mod.AppendCode(R"(
    f ::= () -> (i64, i64) { return 1, 2 }
    g ::= () -> i64 { return 1 }
    h ::= () -> () { }
    'f * 1
    'g * 1
    'h * 1
  )");
  EXPECT_THAT(mod.consumer.diagnostics(),
              UnorderedElementsAre(
                  Pair("type-error", "unexpanded-binary-operator-argument"),
                  Pair("type-error", "unexpanded-binary-operator-argument")));
}

// TODO: Assignment operator overloading tests.

}  // namespace
}  // namespace compiler
