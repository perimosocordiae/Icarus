#include "compiler/compiler.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/module.h"

namespace compiler {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(Declaration, DefaultInitSuccess) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n: i64
    )");
    auto qts = mod.context().qual_types(mod.Append<ast::Identifier>("n"));
    EXPECT_THAT(qts, UnorderedElementsAre(
                         type::QualType(type::I64, type::Quals::Ref())));
    EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n :: i64
    )");
    auto qts = mod.context().qual_types(mod.Append<ast::Identifier>("n"));
    EXPECT_THAT(qts, UnorderedElementsAre(
                         type::QualType(type::I64, type::Quals::Const())));
    EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
  }
}

TEST(Declaration, DefaultInitTypeNotAType) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n: 3 
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(Pair("type-error", "not-a-type")));
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n :: 3 
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(Pair("type-error", "not-a-type")));
  }
}

TEST(Declaration, DefaultInitNonConstantType) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    T := i64
    n: T 
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(
                    Pair("type-error", "non-constant-type-in-declaration")));
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    T := i64
    n :: T
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(
                    Pair("type-error", "non-constant-type-in-declaration")));
  }
}

// TODO Default initialization of non-default-initializable type (both const and
// non-const).

TEST(Declaration, InferredSuccess) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n := 3
    )");
    auto qts = mod.context().qual_types(mod.Append<ast::Identifier>("n"));
    EXPECT_THAT(qts, UnorderedElementsAre(
                         type::QualType(type::I64, type::Quals::Ref())));
    EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n ::= 3
    )");
    auto qts = mod.context().qual_types(mod.Append<ast::Identifier>("n"));
    EXPECT_THAT(qts, UnorderedElementsAre(
                         type::QualType(type::I64, type::Quals::Const())));
    EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
  }
}

TEST(Declaration, InferredUninferralbe) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n := []
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(Pair("type-error", "uninferrable-type")));
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n ::= []
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(Pair("type-error", "uninferrable-type")));
  }
}

TEST(Declaration, InferredAndUninitialized) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n := --
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(Pair("type-error", "uninferrable-type")));
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n ::= --
    )");
    EXPECT_THAT(
        mod.consumer.diagnostics(),
        UnorderedElementsAre(Pair("type-error", "uninferrable-type"),
                             Pair("type-error", "uninitialized-constant")));
  }
}

TEST(Declaration, CustomInitSuccess) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n: i64 = 3
    )");
    auto qts = mod.context().qual_types(mod.Append<ast::Identifier>("n"));
    EXPECT_THAT(qts, UnorderedElementsAre(
                         type::QualType(type::I64, type::Quals::Ref())));
    EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n :: i64 = 3
    )");
    auto qts = mod.context().qual_types(mod.Append<ast::Identifier>("n"));
    EXPECT_THAT(qts, UnorderedElementsAre(
                         type::QualType(type::I64, type::Quals::Const())));
    EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
  }
}

TEST(Declaration, CustomInitTypeNotAType) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n: 3 = 4
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(Pair("type-error", "not-a-type")));
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n :: 3  = 4
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(Pair("type-error", "not-a-type")));
  }
}

TEST(Declaration, CustomInitNonConstantType) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    T := i64
    n: T = 3
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(
                    Pair("type-error", "non-constant-type-in-declaration")));
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    T := i64
    n :: T = 3
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(
                    Pair("type-error", "non-constant-type-in-declaration")));
  }
}

TEST(Declaration, CustomInitAllowsConversions) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n: [0; i64] = []
    )");
    auto qts = mod.context().qual_types(mod.Append<ast::Identifier>("n"));
    EXPECT_THAT(qts, UnorderedElementsAre(type::QualType(
                         type::Arr(0, type::I64), type::Quals::Buf())));
    EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n :: [0; i64] = []
    )");
    auto qts = mod.context().qual_types(mod.Append<ast::Identifier>("n"));
    EXPECT_THAT(qts, UnorderedElementsAre(type::QualType(
                         type::Arr(0, type::I64), type::Quals::Const())));
    EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
  }
}

TEST(Declaration, UninitializedSuccess) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n: i64 = --
    )");
    auto qts = mod.context().qual_types(mod.Append<ast::Identifier>("n"));
    EXPECT_THAT(qts, UnorderedElementsAre(
                         type::QualType(type::I64, type::Quals::Ref())));
    EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n :: i64 = --
    )");
    auto qts = mod.context().qual_types(mod.Append<ast::Identifier>("n"));
    EXPECT_THAT(qts, UnorderedElementsAre(
                         type::QualType(type::I64, type::Quals::Const())));
    EXPECT_THAT(
        mod.consumer.diagnostics(),
        UnorderedElementsAre(Pair("type-error", "uninitialized-constant")));
  }
}

TEST(Declaration, UninitializedTypeNotAType) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n: 3 = --
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(Pair("type-error", "not-a-type")));
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    n :: 3  = --
    )");
    EXPECT_THAT(
        mod.consumer.diagnostics(),
        UnorderedElementsAre(Pair("type-error", "not-a-type"),
                             Pair("type-error", "uninitialized-constant")));
  }
}

TEST(Declaration, UninitializedNonConstantType) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    T := i64
    n: T = --
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(
                    Pair("type-error", "non-constant-type-in-declaration")));
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    T := i64
    n :: T = --
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(
                    Pair("type-error", "non-constant-type-in-declaration"),
                    Pair("type-error", "uninitialized-constant")));
  }
}

TEST(Declaration, NonModuleHole) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    -- := 3
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(
                    Pair("type-error", "declaring-hole-as-non-module")));
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    -- ::= 3
    )");
    EXPECT_THAT(mod.consumer.diagnostics(),
                UnorderedElementsAre(
                    Pair("type-error", "declaring-hole-as-non-module")));
  }
}

TEST(Declaration, NoShadowing) {
  test::TestModule mod;
  mod.AppendCode(R"(
    x := 3
    y := 4
    )");
  EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
}

TEST(Declaration, Shadowing) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    x := 3
    x := 4
    )");
    EXPECT_THAT(
        mod.consumer.diagnostics(),
        UnorderedElementsAre(Pair("type-error", "shadowing-declaration")));
  }

  {
    test::TestModule mod;
    mod.AppendCode(R"(
    x := 3
    x := () => 0
    )");
    EXPECT_THAT(
        mod.consumer.diagnostics(),
        UnorderedElementsAre(Pair("type-error", "shadowing-declaration")));
  }
}

TEST(Declaration, FunctionsCanShadow) {
  test::TestModule mod;
  mod.AppendCode(R"(
    f ::= (n: i64) => n
    f ::= (b: bool) => b
    )");
  EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
}

TEST(Declaration, AmbiguouslyCallableFunctionsCannotShadow) {
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    f ::= (n := 0) => n
    f ::= (b := true) => b
    )");
    EXPECT_THAT(
        mod.consumer.diagnostics(),
        UnorderedElementsAre(Pair("type-error", "shadowing-declaration")));
  }
  {
    test::TestModule mod;
    mod.AppendCode(R"(
    f ::= (n: [0; i64]) => 0
    f ::= (b: [0; bool]) => 0
    )");
    EXPECT_THAT(
        mod.consumer.diagnostics(),
        UnorderedElementsAre(Pair("type-error", "shadowing-declaration")));
  }
}

TEST(BindingDeclaration, Success) {
  test::TestModule mod;
  mod.Append<ast::PatternMatch>(R"(true ~ `x)");
  auto const *e = mod.Append<ast::Expression>(R"(x)");
  ASSERT_THAT(mod.context().qual_types(e),
              ElementsAre(type::QualType::Constant(type::Bool)));
  EXPECT_THAT(mod.consumer.diagnostics(), IsEmpty());
}


// TODO check shadowing on generics once you have interfaces implemented.
// TODO Special functions (copy, move, etc)

}  // namespace
}  // namespace compiler
