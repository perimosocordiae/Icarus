#include <cctype>
#include <cmath>

#include "absl/container/flat_hash_map.h"
#include "ast/ast.h"
#include "base/meta.h"
#include "core/builtin.h"
#include "diagnostic/errors.h"
#include "frontend/lex/lex.h"
#include "frontend/lex/numbers.h"
#include "frontend/lex/operators.h"
#include "frontend/lex/syntax.h"
#include "ir/block_def.h"
#include "ir/results.h"
#include "ir/str.h"
#include "type/basic_type.h"
#ifdef ICARUS_MATCHER
#include "match/binding_id.h"
#include "match/binding_node.h"
#endif  // ICARUS_MATCHER

namespace frontend {

absl::flat_hash_map<std::string_view, ast::Hashtag::Builtin> const
    BuiltinHashtagMap = {{"{export}", ast::Hashtag::Builtin::Export},
                         {"{uncopyable}", ast::Hashtag::Builtin::Uncopyable},
                         {"{immovable}", ast::Hashtag::Builtin::Immovable},
                         {"{inline}", ast::Hashtag::Builtin::Inline},
                         {"{no_default}", ast::Hashtag::Builtin::NoDefault}};

namespace {

constexpr inline bool IsLower(char c) { return ('a' <= c and c <= 'z'); }
constexpr inline bool IsUpper(char c) { return ('A' <= c and c <= 'Z'); }
constexpr inline bool IsDigit(char c) { return ('0' <= c and c <= '9'); }

constexpr inline bool IsAlpha(char c) { return IsLower(c) or IsUpper(c); }
constexpr inline bool IsAlphaNumeric(char c) {
  return IsAlpha(c) or IsDigit(c);
}
constexpr inline bool IsWhitespace(char c) {
  return c == ' ' or c == '\t' or c == '\n' or c == '\r';
}
constexpr inline bool IsAlphaOrUnderscore(char c) {
  return IsAlpha(c) or (c == '_');
}
constexpr inline bool IsAlphaNumericOrUnderscore(char c) {
  return IsAlphaNumeric(c) or (c == '_');
}

SourceCursor NextSimpleWord(SourceCursor *cursor) {
  return cursor->ConsumeWhile(IsAlphaNumericOrUnderscore);
}

static absl::flat_hash_map<std::string_view,
                           std::variant<Operator, Syntax>> const kKeywords = {
    {"which", {Operator::Which}},   {"print", {Operator::Print}},
    {"ensure", {Operator::Ensure}}, {"needs", {Operator::Needs}},
    {"import", {Operator::Import}}, {"flags", {Syntax::Flags}},
    {"enum", {Syntax::Enum}},       {"struct", {Syntax::Struct}},
    {"return", {Operator::Return}}, {"goto", {Operator::Goto}},
    {"jump", {Syntax::Jump}},       {"switch", {Syntax::Switch}},
    {"when", {Operator::When}},     {"as", {Operator::As}},
    {"copy", {Operator::Copy}},     {"move", {Operator::Move}}};

static bool BeginsWith(std::string_view prefix, std::string_view s) {
  if (s.size() < prefix.size()) { return false; }
  auto p_iter = prefix.begin();
  auto s_iter = s.begin();
  while (p_iter != prefix.end()) {
    if (*p_iter++ != *s_iter++) { return false; }
  }
  return true;
}

// Note: The order here is somewhat important. Because we choose the first
// match, we cannot, for example, put `:` before `::=`.
static const std::array<
    std::pair<std::string_view, std::variant<Operator, Syntax>>, 45>
    kOps = {{
        {"@", {Operator::At}},         {",", {Operator::Comma}},
        {"[*]", {Operator::BufPtr}},   {"$", {Operator::Eval}},
        {"+=", {Operator::AddEq}},     {"+", {Operator::Add}},
        {"--", {Syntax::Hole}},        {"-=", {Operator::SubEq}},
        {"->", {Operator::Arrow}},     {"-", {Operator::Sub}},
        {"*=", {Operator::MulEq}},     {"*", {Operator::Mul}},
        {"%=", {Operator::ModEq}},     {"%", {Operator::Mod}},
        {"&=", {Operator::AndEq}},     {"&", {Operator::And}},
        {"|=", {Operator::OrEq}},      {"|", {Operator::Or}},
        {"^=", {Operator::XorEq}},     {"^", {Operator::Xor}},
        {">=", {Operator::Ge}},        {">", {Operator::Gt}},
        {"!", {Operator::Not}},        {"::=", {Operator::DoubleColonEq}},
        {":?", {Operator::TypeOf}},    {"::", {Operator::DoubleColon}},
        {":=", {Operator::ColonEq}},   {"..", {Operator::VariadicPack}},
        {":", {Operator::Colon}},      {"<<", {Operator::Yield}},
        {"<=", {Operator::Le}},        {"<", {Operator::Lt}},
        {"!=", {Operator::Ne}},        {"==", {Operator::Eq}},
        {"=>", {Operator::Rocket}},    {"=", {Operator::Assign}},
        {"'", {Operator::Call}},       {"(", {Syntax::LeftParen}},
        {")", {Syntax::RightParen}},   {"[", {Syntax::LeftBracket}},
        {"]", {Syntax::RightBracket}}, {"{", {Syntax::LeftBrace}},
        {"}", {Syntax::RightBrace}},   {";", {Syntax::Semicolon}},
        {".", {Syntax::Dot}},
    }};

Lexeme NextOperator(SourceCursor *cursor, Source *src) {
#ifdef ICARUS_MATCHER
  // TODO "@% is a terrible choice for the operator here, but we can deal with
  // that later.
  if (BeginsWith(match::kMatchPrefix, cursor->view())) {
    cursor->remove_prefix(2);
    auto word_cursor       = NextSimpleWord(cursor);
    std::string_view token = word_cursor.view();
    auto span              = word_cursor.range();
    return Lexeme(
        std::make_unique<match::BindingNode>(match::BindingId{token}, span));
  }
#endif

  for (auto [prefix, x] : kOps) {
    if (BeginsWith(prefix, cursor->view())) {
      auto span = cursor->remove_prefix(prefix.size()).range();
      return std::visit([&](auto x) { return Lexeme(x, span); }, x);
    }
  }
  UNREACHABLE();
}

std::optional<std::pair<SourceRange, Operator>> NextSlashInitiatedToken(
    SourceCursor *cursor, Source *src) {
  SourceRange span;
  span.begin() = cursor->loc();
  cursor->remove_prefix(1);
  // TODO support multi-line comments?
  switch (cursor->view()[0]) {
    case '/':  // line comment
      cursor->ConsumeWhile([](char) { return true; });
      return std::nullopt;
    case '=':
      cursor->remove_prefix(1);
      span.end().line_num = span.begin().line_num;
      span.end().offset   = span.begin().offset + 2;
      return std::pair{span, Operator::DivEq};
    default:
      span.end().line_num = span.begin().line_num;
      span.end().offset   = span.begin().offset + 1;
      return std::pair{span, Operator::Div};
  }
}

static absl::flat_hash_map<std::string_view, type::BasicType> const
    kReservedTypes{{"bool", type::BasicType::Bool},
                   {"int8", type::BasicType::Int8},
                   {"int16", type::BasicType::Int16},
                   {"int32", type::BasicType::Int32},
                   {"int64", type::BasicType::Int64},
                   {"nat8", type::BasicType::Nat8},
                   {"nat16", type::BasicType::Nat16},
                   {"nat32", type::BasicType::Nat32},
                   {"nat64", type::BasicType::Nat64},
                   {"float32", type::BasicType::Float32},
                   {"float64", type::BasicType::Float64},
                   {"type", type::BasicType::Type_},
                   {"module", type::BasicType::Module},
                   {"byte_view", type::BasicType::ByteView}};

static absl::flat_hash_map<std::string_view, core::Builtin> const kBuiltinFns{
#define ICARUS_CORE_BUILTIN_X(enumerator, str, t)                              \
  {str, core::Builtin::enumerator},
#include "core/builtin.xmacro.h"
#undef ICARUS_CORE_BUILTIN_X
};

Lexeme NextWord(SourceCursor *cursor, Source *src) {
  // Match [a-zA-Z_][a-zA-Z0-9_]*
  // We have already matched the first character
  auto word_cursor       = NextSimpleWord(cursor);
  std::string_view token = word_cursor.view();
  auto span              = word_cursor.range();

  if (token == "true") {
    return Lexeme(std::make_unique<ast::Terminal>(std::move(span), true,
                                                  type::BasicType::Bool));
  } else if (token == "false") {
    return Lexeme(std::make_unique<ast::Terminal>(std::move(span), false,
                                                  type::BasicType::Bool));
  } else if (token == "null") {
    return Lexeme(std::make_unique<ast::Terminal>(
        std::move(span), ir::Addr::Null(), type::BasicType::NullPtr));
  }

  if (auto iter = kReservedTypes.find(token); iter != kReservedTypes.end()) {
    return Lexeme(std::make_unique<ast::Terminal>(std::move(span), iter->second,
                                                  type::BasicType::Type_));
  }

  if (auto iter = kBuiltinFns.find(token); iter != kBuiltinFns.end()) {
    return Lexeme(std::make_unique<ast::BuiltinFn>(span, iter->second));
  }

  if (auto iter = kKeywords.find(token); iter != kKeywords.end()) {
    return std::visit([&](auto x) { return Lexeme(x, span); }, iter->second);
  }

  // "block" is special because it is also the name of the type of such a block.
  // That is, `block { ... }` has type `block`. This means that a function
  // returning a block will look like `() -> block { ... }` and there is an
  // ambiguity whereby we can't tell if this should be parsed as
  // A: () -> (block { ... }), or
  // B: (() -> block) { ... }
  //
  // We can fix this in the parser easily (by checking for a `->` beforehand
  // and prefering (B). Users can specifically add parentheses to get (A), but
  // this requires tagging "block" differently from the other block-head
  // keywords.
  if (token == "block") { return Lexeme(Syntax::Block, span); }
  if (token == "scope") { return Lexeme(Syntax::Scope, span); }

  return Lexeme(std::make_unique<ast::Identifier>(span, std::string{token}));
}

struct StringLiteralLexResult {
  std::string value;
  SourceRange range;
  std::vector<StringLiteralError> errors;
};

StringLiteralLexResult NextStringLiteral(SourceCursor *cursor, Source *src) {
  StringLiteralLexResult result;
  cursor->remove_prefix(1);
  bool escaped        = false;
  int offset          = -1;
  auto str_lit_cursor = cursor->ConsumeWhile([&](char c) {
    ++offset;
    if (not escaped) {
      switch (c) {
        case '\\': escaped = true; return true;
        case '"': return false;
        // TODO non-printable chars?
        default: result.value.push_back(c); return true;
      }
    }

    escaped = false;
    switch (c) {
      case '\\':
      case '"': result.value.push_back(c); break;
      case 'a': result.value.push_back('\a'); break;
      case 'b': result.value.push_back('\b'); break;
      case 'f': result.value.push_back('\f'); break;
      case 'n': result.value.push_back('\n'); break;
      case 'r': result.value.push_back('\r'); break;
      case 't': result.value.push_back('\t'); break;
      case 'v': result.value.push_back('\v'); break;
      default: {
        result.errors.push_back(StringLiteralError{
            .kind   = StringLiteralError::Kind::kInvalidEscapedChar,
            .offset = offset,
        });
      } break;
    }

    return true;
  });

  result.range = str_lit_cursor.range();
  if (cursor->view().empty()) {
    result.errors.push_back(StringLiteralError{
        .kind   = StringLiteralError::Kind::kRunaway,
        .offset = -1,
    });
  } else {
    cursor->remove_prefix(1);  // Ending '"'
  }

  return result;
}

enum class HashtagError {
  kNothingAfterHash,
  kMissingCloseBraceOnSystemHashtag,
  kUnrecognizedHashtag,
  kNotYetSupportedUserDefinedHashtag,
};

base::expected<Lexeme, HashtagError> NextHashtag(SourceCursor *cursor,
                                                 Source *src) {
  SourceRange span;
  std::string_view token;
  if (cursor->view().empty()) {
    return HashtagError::kNothingAfterHash;
  } else if (cursor->view()[0] == '{') {
    cursor->remove_prefix(1);
    auto word_cursor = NextSimpleWord(cursor);
    token            = std::string_view{word_cursor.view().data() - 1,
                             word_cursor.view().size() + 2};
    span             = word_cursor.range();

    if (cursor->view().empty() or cursor->view()[0] != '}') {
      return HashtagError::kMissingCloseBraceOnSystemHashtag;
    }
    cursor->remove_prefix(1);
    span = span.expanded(Offset(1));

    if (auto iter = BuiltinHashtagMap.find(token);
        iter != BuiltinHashtagMap.end()) {
      return Lexeme(ast::Hashtag{iter->second}, span);
    }

    return HashtagError::kUnrecognizedHashtag;
  } else {
    auto word_cursor = NextSimpleWord(cursor);
    token            = word_cursor.view();
    span             = word_cursor.range();

    // TODO
    return HashtagError::kNotYetSupportedUserDefinedHashtag;
  }
}

Lexeme NextNumber(SourceCursor *cursor, Source *src,
                  diagnostic::DiagnosticConsumer &diag) {
  // TODO hex-parsing?
  auto num_cursor = cursor->ConsumeWhile([](char c) {
    return c == 'b' or c == 'o' or c == 'd' or c == 'd' or c == '_' or
           c == '.' or IsDigit(c);
  });

  auto span = num_cursor.range();
  return std::visit(
      [&](auto num) {
        using T = std::decay_t<decltype(num)>;
        if constexpr (std::is_same_v<T, int64_t>) {
          return Lexeme(std::make_unique<ast::Terminal>(
              std::move(span), num, type::BasicType::Int64));
        } else if constexpr (std::is_same_v<T, double>) {
          return Lexeme(std::make_unique<ast::Terminal>(
              std::move(span), num, type::BasicType::Float64));
        } else if constexpr (std::is_same_v<T, NumberParsingError>) {
          // Even though we could try to be helpful by guessing the type, it's
          // unlikely to be useful. The value may also be important if it's used
          // at compile-time (e.g., as an array extent). Generally proceeding
          // further if we can't lex the input is likely not going to be useful.
          diag.Consume(diagnostic::NumberParsingFailure{
              .error = num,
              .range = span,
          });
          return Lexeme(std::make_unique<ast::Terminal>(
              std::move(span), 0, type::BasicType::Int32));
        } else {
          static_assert(base::always_false<T>());
        }
      },
      ParseNumber(num_cursor.view()));
}
}  // namespace

Lexeme NextToken(LexState *state) {
restart:
  // Delegate based on the next character in the file stream
  if (state->cursor_.view().empty()) {
    auto chunk = state->src_->ReadUntil('\n');
    if (chunk.more_to_read) {
      state->cursor_ =
          SourceCursor(state->cursor_.loc().next_line(), chunk.view);
      return Lexeme(Syntax::ImplicitNewline,
                    state->cursor_.remove_prefix(0).range());
    } else {
      return Lexeme(Syntax::EndOfFile, state->cursor_.remove_prefix(0).range());
    }
  } else if (IsAlphaOrUnderscore(state->peek())) {
    return NextWord(&state->cursor_, state->src_);
  } else if (IsDigit(state->peek()) or
             (state->peek() == '.' and state->cursor_.view().size() > 1 and
              IsDigit(state->cursor_.view()[1]))) {
    return NextNumber(&state->cursor_, state->src_, state->diag_);
  }

  char peek = state->peek();
  if (static_cast<uint8_t>(peek) >= 0x80 or not std::isprint(peek)) {
    auto loc = state->cursor_.loc();
    state->cursor_.remove_prefix(1);
    state->diag_.Consume(diagnostic::UnprintableSourceCharacter{
        .value = peek,
        .range = SourceRange(loc, loc + Offset(1)),
    });
    goto restart;
  }
  switch (peek) {
    case '"': {
      auto [str, range, errors] =
          NextStringLiteral(&state->cursor_, state->src_);
      if (not errors.empty()) {
        state->diag_.Consume(diagnostic::StringLiteralParsingFailure{
            .errors = std::move(errors),
            .range  = range,
        });
      }
      return Lexeme(std::make_unique<ast::Terminal>(std::move(range),
                                                    ir::SaveStringGlobally(str),
                                                    type::BasicType::ByteView));

    } break;
    case '#': {
      state->cursor_.remove_prefix(1);
      if (state->peek() == '.') {
        state->cursor_.remove_prefix(1);
        auto word_cursor       = NextSimpleWord(&state->cursor_);
        std::string_view token = word_cursor.view();
        auto range             = word_cursor.range();

        return Lexeme(std::make_unique<ast::Label>(word_cursor.range(),
                                                   std::string{token}));

      } else {
        if (auto result = NextHashtag(&state->cursor_, state->src_)) {
          return *std::move(result);
        }

        state->diag_.Consume(diagnostic::HashtagParsingFailure{});
        goto restart;
      }
    } break;
    case '/': {
      // TODO just check for comments early and roll this into NextOperator.
      if (auto maybe_op =
              NextSlashInitiatedToken(&state->cursor_, state->src_)) {
        auto &[span, op] = *maybe_op;
        return Lexeme(op, state->cursor_.range());
      }
      goto restart;
    } break;
    case '\n':
    case '\r':
    case '\v':
    case '\t':
    case ' ': state->cursor_.ConsumeWhile(IsWhitespace); goto restart;
    case '`':
    case '~':
    case '?': {
      auto loc = state->cursor_.loc();
      state->cursor_.remove_prefix(1);
      state->diag_.Consume(diagnostic::InvalidSourceCharacter{
          .value = peek,
          .range = SourceRange(loc, loc + Offset(1)),
      });
      goto restart;
    }
    case '\\': {
      if (state->cursor_.view().size() >= 2 and
          state->cursor_.view()[1] == '\\') {
        return Lexeme(Syntax::ExplicitNewline,
                      state->cursor_.remove_prefix(2).range());
      }
      auto span = state->cursor_.remove_prefix(1).range();
      state->cursor_.ConsumeWhile(IsWhitespace);
      if (not state->cursor_.view().empty()) {
        state->diag_.Consume(
            diagnostic::NonWhitespaceAfterNewlineEscape{.range = span});
      }
      goto restart;
    } break;
    default: return NextOperator(&state->cursor_, state->src_); break;
  }
  UNREACHABLE();
}

}  // namespace frontend