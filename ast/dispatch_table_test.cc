#include "ast/dispatch_table.h"

#include <string_view>

#include "ast/ast.h"
#include "ast/overload_set.h"
#include "test/catch.h"
#include "test/util.h"
#include "type/function.h"
#include "type/pointer.h"
#include "type/variant.h"

// TODO reenable when you figure out a better way to generate type-verified code
// from strings for testing purposes.
//
// namespace ast {
// namespace {
// core::FnArgs<std::pair<Expression const*, compiler::VerifyResult>>
// ResultsForArgs(compiler::Compiler* visitor,
//                core::FnArgs<Expression const*> const& args) {
//   return args.Transform([visitor](Expression const* expr) {
//     return std::pair<Expression const*, compiler::VerifyResult>(
//         expr, *ASSERT_NOT_NULL(visitor->prior_verification_attempt(expr)));
//   });
// }
// 
// core::OrderedFnArgs<Expression> MakeFnArgs(
//     std::vector<std::string> pos_args,
//     absl::flat_hash_map<std::string, std::string> named_args) {
//   std::vector<std::pair<std::string, std::unique_ptr<Expression>>> vec;
//   for (auto pos_arg : pos_args) {
//     vec.emplace_back("", test::MakeVerified<Expression>(std::move(pos_arg)));
//   }
//   for (auto& [name, arg] : named_args) {
//     vec.emplace_back(std::move(name),
//                      test::MakeVerified<Expression>(std::move(arg)));
//   }
//   return core::OrderedFnArgs<Expression>(std::move(vec));
// }
// 
// std::pair<OverloadSet, std::unique_ptr<Call>> MakeCall(
//     std::unique_ptr<Expression> fn, std::vector<std::string> pos_args,
//     absl::flat_hash_map<std::string, std::string> named_args) {
//   auto call_expr = std::make_unique<Call>(
//       frontend::SourceRange{}, std::move(fn),
//       MakeFnArgs(std::move(pos_args), std::move(named_args)));
//   OverloadSet os;
//   os.emplace(call_expr->callee(),
//              compiler::VerifyResult::Constant(type::Func({}, {})));
// 
//   return std::pair(std::move(os), std::move(call_expr));
// }
// 
// // TODO you're not really handling const-ness correctly in any of these tests.
// TEST_CASE("() -> ()") {
//   auto [src, mod, v, fn] = test::MakeVerified<Expression>("() -> () {}");
// 
//   auto call_with = [&](std::vector<std::string>&& pos,
//                        absl::flat_hash_map<std::string, std::string>&& named) {
//     return MakeCall(std::move(fn), std::move(pos), std::move(named));
//   };
// 
//   SECTION("without args") {
//     auto [os, call_expr] = call_with({}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Void()));
//   }
// 
//   SECTION("with positional arg") {
//     auto [os, call_expr] = call_with({"true"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with multiple positional args") {
//     auto [os, call_expr] = call_with({"true", "3"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with named arg") {
//     auto [os, call_expr] = call_with({}, {{"b", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with multiple named args") {
//     auto [os, call_expr] = call_with({}, {{"b", "true"}, {"n", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with both positional and named args") {
//     auto [os, call_expr] = call_with({"true"}, {{"n", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// }
// 
// TEST_CASE("(b: bool) -> ()") {
//   auto [src, mod, v, fn] = test::MakeVerified<Expression>("(b: bool) -> () {}");
// 
//   auto call_with = [&](std::vector<std::string>&& pos,
//                         absl::flat_hash_map<std::string, std::string>&& named) {
//     return MakeCall(std::move(fn), std::move(pos), std::move(named));
//   };
// 
//   SECTION("without args") {
//     auto [os, call_expr] = call_with({}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one positional arg - correct type") {
//     auto [os, call_expr] = call_with({"true"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Void()));
//   }
// 
//   SECTION("with one positional arg - incorrect type") {
//     auto [os, call_expr] = call_with({"3"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one named arg - correct type, correct name") {
//     auto [os, call_expr] = call_with({}, {{"b", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Void()));
//   }
// 
//   SECTION("with one named arg - incorrect type, correct name") {
//     auto [os, call_expr] = call_with({}, {{"b", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one named arg - correct type, incorrect name") {
//     auto [os, call_expr] = call_with({}, {{"n", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one named arg - incorrect type, incorrect name") {
//     auto [os, call_expr] = call_with({}, {{"n", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with multiple positional args") {
//     auto [os, call_expr] = call_with({"true", "3"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with multiple named args") {
//     auto [os, call_expr] = call_with({}, {{"b", "true"}, {"n", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with both positional and named args") {
//     auto [os, call_expr] = call_with({"true"}, {{"n", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with named arg shadowing positional arg - correct type") {
//     auto [os, call_expr] = call_with({"true"}, {{"b", "false"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with named arg shadowing positional arg - incorrect type") {
//     auto [os, call_expr] = call_with({"true"}, {{"b", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// }
// 
// TEST_CASE("(b := true) -> ()") {
//   auto [src, mod, v, fn] = test::MakeVerified<Expression>("(b := true) -> () {}");
// 
//   auto call_with = [&](std::vector<std::string>&& pos,
//                        absl::flat_hash_map<std::string, std::string>&& named) {
//     return MakeCall(std::move(fn), std::move(pos), std::move(named));
//   };
// 
//   SECTION("without args") {
//     auto [os, call_expr] = call_with({}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Void()));
//   }
// 
//   SECTION("with one positional arg - correct type") {
//     auto [os, call_expr] = call_with({"true"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Void()));
//   }
// 
//   SECTION("with one positional arg - incorrect type") {
//     auto [os, call_expr] = call_with({"3"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one named arg - correct type, correct name") {
//     auto [os, call_expr] = call_with({}, {{"b", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Void()));
//   }
// 
//   SECTION("with one named arg - incorrect type, correct name") {
//     auto [os, call_expr] = call_with({}, {{"b", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one named arg - correct type, incorrect name") {
//     auto [os, call_expr] = call_with({}, {{"n", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one named arg - incorrect type, incorrect name") {
//     auto [os, call_expr] = call_with({}, {{"n", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with multiple positional args") {
//     auto [os, call_expr] = call_with({"true", "3"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with multiple named args") {
//     auto [os, call_expr] = call_with({}, {{"b", "true"}, {"n", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with both positional and named args") {
//     auto [os, call_expr] = call_with({"true"}, {{"n", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with named arg shadowing positional arg - correct type") {
//     auto [os, call_expr] = call_with({"true"}, {{"b", "false"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with named arg shadowing positional arg - incorrect type") {
//     auto [os, call_expr] = call_with({"true"}, {{"b", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// }
// 
// TEST_CASE("(n: int64, b: bool) -> ()") {
//   auto [src, mod, v, fn] =
//       test::MakeVerified<Expression>("(n: int64, b: bool) -> ()");
// 
//   auto call_with = [&](std::vector<std::string>&& pos,
//                         absl::flat_hash_map<std::string, std::string>&& named) {
//     return MakeCall(std::move(fn), std::move(pos), std::move(named));
//   };
// 
//   SECTION("without args") {
//     auto [os, call_expr] = call_with({}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one positional arg - correct type") {
//     auto [os, call_expr] = call_with({"true"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one positional arg - incorrect type") {
//     auto [os, call_expr] = call_with({"3"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with multiple positional args - incorrect order") {
//     auto [os, call_expr] = call_with({"true", "3"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with multiple positional args - correct order") {
//     auto [os, call_expr] = call_with({"3", "true"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Void()));
//   }
// 
//   SECTION("with one named arg") {
//     auto [os, call_expr] = call_with({}, {{"b", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with multiple named args - correct types, correct names ") {
//     auto [os, call_expr] = call_with({}, {{"b", "true"}, {"n", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Void()));
//   }
// 
//   SECTION("with multiple named args - incorrect types, correct names") {
//     auto [os, call_expr] = call_with({}, {{"b", "true"}, {"n", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with multiple named args - correct types, incorrect names") {
//     auto [os, call_expr] = call_with({}, {{"B", "true"}, {"n", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with multiple named args - incorrect types, incorrect names") {
//     auto [os, call_expr] = call_with({}, {{"B", "true"}, {"n", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with both positional and named args") {
//     auto [os, call_expr] = call_with({"3"}, {{"b", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Void()));
//   }
// 
//   SECTION("with both positional and named args - named has incorrect type") {
//     auto [os, call_expr] = call_with({"3"}, {{"b", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION(
//       "with both positional and named args - positional has incorrect type") {
//     auto [os, call_expr] = call_with({"true"}, {{"b", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with both positional and named args - both have  incorrect type") {
//     auto [os, call_expr] = call_with({"true"}, {{"b", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// }
// 
// TEST_CASE("(T :: type) -> *T") {
//   auto [src, mod, v, fn] = test::MakeVerified<Expression>("(T :: type) -> *T");
// 
//   auto call_with = [&](std::vector<std::string>&& pos,
//                        absl::flat_hash_map<std::string, std::string>&& named) {
//     return MakeCall(std::move(fn), std::move(pos), std::move(named));
//   };
// 
//   SECTION("without args") {
//     auto [os, call_expr] = call_with({}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one positional arg - (bool)") {
//     auto [os, call_expr] = call_with({"bool"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Ptr(type::Bool)));
//   }
// 
//   SECTION("with one positional arg - (int64)") {
//     auto [os, call_expr] = call_with({"int64"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Ptr(type::Int64)));
//   }
// 
//   SECTION("with one named arg - (T = bool)") {
//     auto [os, call_expr] = call_with({}, {{"T", "bool"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Ptr(type::Bool)));
//   }
// 
//   SECTION("with one named arg - (t = bool)") {
//     auto [os, call_expr] = call_with({}, {{"t", "bool"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one named arg - (T = true)") {
//     auto [os, call_expr] = call_with({}, {{"T", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one named arg - (t = true)") {
//     auto [os, call_expr] = call_with({}, {{"t", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// }
// 
// TEST_CASE("(T ::= bool) -> *T") {
//   auto [src, mod, v, fn] = test::MakeVerified<Expression>("(T ::= bool) -> *T");
// 
//   auto call_with = [&](std::vector<std::string>&& pos,
//                         absl::flat_hash_map<std::string, std::string>&& named) {
//     return MakeCall(std::move(fn), std::move(pos), std::move(named));
//   };
// 
//   SECTION("without args") {
//     auto [os, call_expr] = call_with({}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Ptr(type::Bool)));
//   }
// 
//   SECTION("with one positional arg - correct type") {
//     auto [os, call_expr] = call_with({"bool"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Ptr(type::Bool)));
//   }
// 
//   SECTION("with one positional arg - incorrect type") {
//     auto [os, call_expr] = call_with({"int64"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Ptr(type::Int64)));
//   }
// 
//   SECTION("with one named arg - correct type, correct name") {
//     auto [os, call_expr] = call_with({}, {{"T", "bool"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Ptr(type::Bool)));
//   }
// 
//   SECTION("with one named arg - correct type, incorrect name") {
//     auto [os, call_expr] = call_with({}, {{"t", "bool"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one named arg - incorrect type, correct name") {
//     auto [os, call_expr] = call_with({}, {{"T", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one named arg - incorrect type, incorrect name") {
//     auto [os, call_expr] = call_with({}, {{"t", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// }
// 
// TEST_CASE("(val: T, T ::= bool) -> *T") {
//   auto [src, mod, v, fn] =
//       test::MakeVerified<Expression>("(val: T, T ::= bool) -> *T");
// 
//   auto call_with = [&](std::vector<std::string>&& pos,
//                        absl::flat_hash_map<std::string, std::string>&& named) {
//     return MakeCall(std::move(fn), std::move(pos), std::move(named));
//   };
//   SECTION("without args") {
//     auto [os, call_expr] = call_with({}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one positional arg - correct type") {
//     auto [os, call_expr] = call_with({"true"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Ptr(type::Bool)));
//   }
// 
//   SECTION("with one positional arg - incorrect type") {
//     auto [os, call_expr] = call_with({"3"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one named arg - correct type, correct name") {
//     auto [os, call_expr] = call_with({}, {{"val", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Ptr(type::Bool)));
//   }
// 
//   SECTION("with one named arg - correct type, incorrect name") {
//     auto [os, call_expr] = call_with({}, {{"x", "true"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one named arg - incorrect type, correct name") {
//     auto [os, call_expr] = call_with({}, {{"val", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with one named arg - incorrect type, incorrect name") {
//     auto [os, call_expr] = call_with({}, {{"x", "3"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with multiple positional arguments - (bool)") {
//     auto [os, call_expr] = call_with({"true", "bool"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Ptr(type::Bool)));
//   }
// 
//   SECTION("with multiple positional arguments - (int64)") {
//     auto [os, call_expr] = call_with({"3", "int64"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Ptr(type::Int64)));
//   }
// 
//   SECTION("with multiple positional arguments - type mismatch") {
//     auto [os, call_expr] = call_with({"3", "bool"}, {});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("with multiple named arguments - (bool)") {
//     auto [os, call_expr] = call_with({}, {{"val", "true"}, {"T", "bool"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Ptr(type::Bool)));
//   }
// 
//   SECTION("with multiple named arguments - (int64)") {
//     auto [os, call_expr] = call_with({}, {{"val", "3"}, {"T", "int64"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Ptr(type::Int64)));
//   }
// 
//   SECTION("with both positional and named args - correct types") {
//     auto [os, call_expr] = call_with({"3"}, {{"T", "int64"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Constant(type::Ptr(type::Int64)));
//   }
// 
//   SECTION("with both positional and named args - incorrect types") {
//     auto [os, call_expr] = call_with({"true"}, {{"T", "int64"}});
//     CHECK(VerifyDispatch(&v, call_expr.get(), os,
//                          ResultsForArgs(&v, call_expr->args())) ==
//           compiler::VerifyResult::Error());
//   }
// }
// 
// TEST_CASE("overload set") {
//   auto [src, mod, v, node] = test::MakeVerified<Node>(R"(
//     f ::= (val: bool) -> bool {}
//     f ::= (val: int64) -> int64 {}
//   )");
// 
//   auto bool_overload = test::MakeVerified<Expression>(src);
//   auto int_overload  = test::MakeVerified<Expression>(src);
// 
//   OverloadSet os;
//   os.emplace(bool_overload.get(),
//              compiler::VerifyResult::Constant(type::Generic));
//   os.emplace(int_overload.get(),
//              compiler::VerifyResult::Constant(type::Generic));
// 
//   SECTION("without args") {
//     Call call_expr(frontend::SourceRange{},
//                    test::MakeUnverified<Expression>("f"), MakeFnArgs({}, {}));
//     CHECK(VerifyDispatch(&v, &call_expr, os,
//                          ResultsForArgs(&v, call_expr.args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("without one positional arg - bool") {
//     Call call_expr(frontend::SourceRange{},
//                    test::MakeUnverified<Expression>("f"),
//                    MakeFnArgs({"true"}, {}));
//     CHECK(VerifyDispatch(&v, &call_expr, os,
//                          ResultsForArgs(&v, call_expr.args())) ==
//           compiler::VerifyResult::Constant(type::Bool));
//   }
// 
//   SECTION("without one positional arg - int64") {
//     Call call_expr(frontend::SourceRange{},
//                    test::MakeUnverified<Expression>("f"),
//                    MakeFnArgs({"3"}, {}));
//     CHECK(VerifyDispatch(&v, &call_expr, os,
//                          ResultsForArgs(&v, call_expr.args())) ==
//           compiler::VerifyResult::Constant(type::Int64));
//   }
// 
//   SECTION("without one positional arg - no matching type") {
//     Call call_expr(frontend::SourceRange{},
//                    test::MakeUnverified<Expression>("f"),
//                    MakeFnArgs({"3.0"}, {}));
//     CHECK(VerifyDispatch(&v, &call_expr, os,
//                          ResultsForArgs(&v, call_expr.args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("without one named arg - bool") {
//     Call call_expr(frontend::SourceRange{},
//                    test::MakeUnverified<Expression>("f"),
//                    MakeFnArgs({}, {{"val", "true"}}));
//     CHECK(VerifyDispatch(&v, &call_expr, os,
//                          ResultsForArgs(&v, call_expr.args())) ==
//           compiler::VerifyResult::Constant(type::Bool));
//   }
// 
//   SECTION("without one named arg - int64") {
//     Call call_expr(frontend::SourceRange{},
//                    test::MakeUnverified<Expression>("f"),
//                    MakeFnArgs({}, {{"val", "3"}}));
//     CHECK(VerifyDispatch(&v, &call_expr, os,
//                          ResultsForArgs(&v, call_expr.args())) ==
//           compiler::VerifyResult::Constant(type::Int64));
//   }
// 
//   SECTION("without one named arg - no matching type") {
//     Call call_expr(frontend::SourceRange{},
//                    test::MakeUnverified<Expression>("f"),
//                    MakeFnArgs({}, {{"val", "3.0"}}));
//     CHECK(VerifyDispatch(&v, &call_expr, os,
//                          ResultsForArgs(&v, call_expr.args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("without variant - matching one") {
//     auto variant = test::MakeVerified<Expression>("v: bool | float32 = true");
//     Call call_expr(frontend::SourceRange{},
//                    test::MakeUnverified<Expression>("f"),
//                    MakeFnArgs({"v"}, {}));
//     CHECK(VerifyDispatch(&v, &call_expr, os,
//                          ResultsForArgs(&v, call_expr.args())) ==
//           compiler::VerifyResult::Error());
//   }
// 
//   SECTION("without variant - matching both") {
//     auto variant = test::MakeVerified<Expression>("v: bool | int64 = true");
//     Call call_expr(frontend::SourceRange{},
//                    test::MakeUnverified<Expression>("f"),
//                    MakeFnArgs({"v"}, {}));
//     CHECK(
//         VerifyDispatch(&v, &call_expr, os,
//                        ResultsForArgs(&v, call_expr.args())) ==
//         compiler::VerifyResult::Constant(type::Var({type::Bool, type::Int64})));
//   }
// }
// 
// }  // namespace
// }  // namespace ast
