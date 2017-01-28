#include "Lexer.h"

#ifndef ICARUS_UNITY
#include "Type/Type.h"
#endif

static inline bool IsLower(char c) { return ('a' <= c && c <= 'z'); }
static inline bool IsUpper(char c) { return ('A' <= c && c <= 'Z'); }
static inline bool IsDigit(char c) { return ('0' <= c && c <= '9'); }
static inline bool IsAlpha(char c) { return IsLower(c) || IsUpper(c); }
static inline bool IsAlphaNumeric(char c) { return IsAlpha(c) || IsDigit(c); }
static inline bool IsWhitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
static inline bool IsDigitOrUnderscore(char c) {
  return IsDigit(c) || (c == '_');
}
static inline bool IsAlphaOrUnderscore(char c) {
  return IsAlpha(c) || (c == '_');
}
static inline bool IsAlphaNumericOrUnderscore(char c) {
  return IsAlphaNumeric(c) || (c == '_');
}
// static inline bool IsNewline(char c) { return (char)10 <= c && c <= (char)13; }
static inline bool IsNonGraphic(char c) {
  return c < (char)9 || ((char)13 < c && c < (char)32) || c == (char)127;
}

#define RETURN_TERMINAL(term_type, ty, val)                                    \
  auto term_ptr           = new AST::Terminal;                                 \
  term_ptr->loc           = cursor;                                            \
  term_ptr->terminal_type = Language::Terminal::term_type;                     \
  term_ptr->type          = ty;                                                \
  term_ptr->value = val;                                                       \
  return NNT(term_ptr, Language::expr);

#define RETURN_NNT(tk, nt, tk_len)                                             \
  {                                                                            \
    Cursor loc = cursor;                                                       \
    loc.offset -= tk_len;                                                      \
    return NNT(new AST::TokenNode(loc, tk), Language::nt);                     \
  }

extern std::map<const char *, Type *> PrimitiveTypes;

// Take a filename as a string or a C-string and opens the named file
Lexer::Lexer(SourceFile *sf) : source_file_(sf) {
  char *file_name = new char[source_file_->name.size() + 1];
  strcpy(file_name, source_file_->name.c_str());
  cursor.file_name = file_name;

  ifs = std::ifstream(file_name, std::ifstream::in);

  pstr temp_blank;
  source_file_->lines.push_back(temp_blank); // Blank line since we 1-index.

  MoveCursorToNextLine();
}

Lexer::~Lexer() { ifs.close(); }

// Copy in the next line of text from the file, and scrub away non-graphic
// characters (replacing them with ' '). We leave characters in the range 32-126
// as well as 9-13.
void Lexer::MoveCursorToNextLine() {
  assert(!ifs.eof());
  std::string temp;
  std::getline(ifs, temp);

  // Check for null characters in line
  size_t line_length = temp.size();
  for (size_t i = 0; i < line_length; ++i) {
    if (temp[i] == '\0') {
      temp[i] = ' ';
      ErrorLog::NullCharInSrc(cursor);
    } else if (IsNonGraphic(temp[i])) {
      temp[i] = ' ';
      ErrorLog::NonGraphicCharInSrc(cursor);
    }
  }

  cursor.offset = 0;
  cursor.line   = pstr(temp.c_str());

  ++cursor.line_num;
  source_file_->lines.push_back(cursor.line);
}

void Lexer::IncrementCursor() {
  if (*cursor != '\0') {
    ++cursor.offset;
  } else {
    MoveCursorToNextLine();
  }
}

void Lexer::BackUpCursor() {
  // You can't back up to a previous line.
  assert(cursor.offset > 0);
  --cursor.offset;
}

void Lexer::SkipToEndOfLine() {
  while (*cursor != '\0') { ++cursor.offset; }
}

// Get the next token
NNT Lexer::Next() {
restart:
  // Delegate based on the next character in the file stream
  if (ifs.eof()) {
    RETURN_NNT("", eof, 0);

  } else if (IsAlphaOrUnderscore(*cursor)) {
    return NextWord();

  } else if (IsDigit(*cursor)) {
    return NextNumber();
  }

  switch (*cursor) {
  case '\t':
  case ' ': IncrementCursor(); goto restart; // Explicit TCO
  case '\0': IncrementCursor(); RETURN_NNT("", newline, 0);
  default: return NextOperator();
  }
}

NNT Lexer::NextWord() {
  // Match [a-zA-Z_][a-zA-Z0-9_]*
  // We have already matched the first character

  auto starting_offset = cursor.offset;

  do {
    IncrementCursor();
  } while (IsAlphaNumericOrUnderscore(*cursor));

  char old_char     = *cursor;
  *cursor           = '\0';
  std::string token = cursor.line.ptr + starting_offset;
  *cursor           = old_char;

  // Check if the word is a type primitive/literal and if so, build the
  // appropriate Node.
  for (const auto &type_lit : PrimitiveTypes) {
    if (type_lit.first == token) {
      RETURN_TERMINAL(Type, Type_, IR::Value::Type(type_lit.second));
    }
  }

  if (token == "true" || token == "false") {
    RETURN_TERMINAL(True, Bool, IR::Value::Bool(token == "true"));

  } else if (token == "null") {
    RETURN_TERMINAL(Null, NullPtr, IR::Value::None());

  } else if (token == "ord") {
    RETURN_TERMINAL(Ord, Func(Char, Uint), IR::Value::None());

  } else if (token == "ascii") {
    RETURN_TERMINAL(ASCII, Func(Uint, Char), IR::Value::None());

  } else if (token == "else") {
    auto term_ptr           = new AST::Terminal;
    Cursor loc              = cursor;
    loc.offset              = starting_offset;
    term_ptr->loc           = loc;
    term_ptr->terminal_type = Language::Terminal::Else;
    term_ptr->type          = Bool;

    return NNT(term_ptr, Language::kw_else);
  }

#define CASE_RETURN_NNT(str, op, len)                                          \
  if (strcmp(token_cstr, str) == 0) { RETURN_NNT(str, op, len); }

  auto token_cstr = token.c_str();

  CASE_RETURN_NNT("in",       op_b,          2)
  CASE_RETURN_NNT("print",    op_l,          5)
  CASE_RETURN_NNT("import",   op_l,          6)
  CASE_RETURN_NNT("free",     op_l,          4)
  CASE_RETURN_NNT("while",    kw_expr_block, 5)
  CASE_RETURN_NNT("for",      kw_expr_block, 3)
  CASE_RETURN_NNT("if",       kw_if,         2)
  CASE_RETURN_NNT("case",     kw_block,      4)
  CASE_RETURN_NNT("enum",     kw_block,      4)
  CASE_RETURN_NNT("struct",   kw_struct,     6)
  CASE_RETURN_NNT("return",   op_lt,         6)
  CASE_RETURN_NNT("continue", op_lt,         8)
  CASE_RETURN_NNT("break",    op_lt,         5)
  CASE_RETURN_NNT("repeat",   op_lt,         6)
  CASE_RETURN_NNT("restart",  op_lt,         7)

#undef CASE_RETURN_NNT

  Cursor loc = cursor;
  loc.offset = starting_offset;
  return NNT(new AST::Identifier(loc, token), Language::expr);
}

template <u64 Base> u64 ToDigit(char c);
template <> u64 ToDigit<10>(char c) { return static_cast<u64>(c - '0'); }
template <> u64 ToDigit<8>(char c) { return static_cast<u64>(c - '0'); }
template <> u64 ToDigit<2>(char c) { return static_cast<u64>(c - '0'); }
template <> u64 ToDigit<16>(char c) {
  if ('a' <= c && c <= 'f') { return static_cast<u64>(c - 'a' + 10); }
  if ('A' <= c && c <= 'F') { return static_cast<u64>(c - 'A' + 10); }
  return static_cast<u64>(c - '0');
}

template <u64 Base> u64 ParseInt(char *head, char *end) {
  u64 result = 0;
  for(; head < end; ++head) {
    if (*head == '_') { continue; }
    result *= Base;
    result += ToDigit<Base>(*head);
  }
  return result;
}

NNT Lexer::NextNumber() {
  auto starting_offset = cursor.offset;

  do { IncrementCursor(); } while (IsDigitOrUnderscore(*cursor));

  switch (*cursor) {
  case 'u':
  case 'U': {
    u64 val = ParseInt<10>(cursor.line.ptr + starting_offset,
                           cursor.line.ptr + cursor.offset);
    IncrementCursor();
    RETURN_TERMINAL(Uint, Uint, IR::Value::Uint(val));
  } break;

  case '.': {
    IncrementCursor();
    if (*cursor == '.') {
      BackUpCursor();
      i64 val = static_cast<i64>(ParseInt<10>(cursor.line.ptr + starting_offset,
                                              cursor.line.ptr + cursor.offset));
      RETURN_TERMINAL(Int, Int, IR::Value::Int(val));
    }

    // Just one dot. Should be interpretted as a decimal point.
    IncrementCursor();
    while (IsDigitOrUnderscore(*cursor)) { IncrementCursor(); }

    char old_char = *cursor;
    *cursor       = '\0';
    auto real_val = std::stod(cursor.line.ptr + starting_offset);
    *cursor       = old_char;

    RETURN_TERMINAL(Real, Real, IR::Value::Real(real_val));
  } break;

  default: {
    i64 val = static_cast<i64>(ParseInt<10>(cursor.line.ptr + starting_offset,
                                            cursor.line.ptr + cursor.offset));
    RETURN_TERMINAL(Int, Int, IR::Value::Int(val));  } break;
  }
}

NNT Lexer::NextOperator() {
  switch (*cursor) {
  case '`': IncrementCursor(); RETURN_NNT("`", op_bl, 1);
  case '@': IncrementCursor(); RETURN_NNT("@", op_l, 1);
  case ',': IncrementCursor(); RETURN_NNT(",", comma, 1);
  case ';': IncrementCursor(); RETURN_NNT(";", semicolon, 1);
  case '(': IncrementCursor(); RETURN_NNT("(", l_paren, 1);
  case ')': IncrementCursor(); RETURN_NNT(")", r_paren, 1);
  case '[': IncrementCursor(); RETURN_NNT("[", l_bracket, 1);
  case ']': IncrementCursor(); RETURN_NNT("]", r_bracket, 1);
  case '{': IncrementCursor(); RETURN_NNT("{", l_brace, 1);
  case '}': IncrementCursor(); RETURN_NNT("}", r_brace, 1);
  case '$': IncrementCursor(); RETURN_NNT("$", op_l, 1);

  case '.': {
    Cursor cursor_copy  = cursor;

    // Note: safe because we know we have a null-terminator
    while (*cursor == '.') { IncrementCursor(); }
    size_t num_dots = cursor.offset - cursor_copy.offset;

    if (num_dots == 1) {
      RETURN_NNT(".", op_b, 1);
    } else {
      if (num_dots > 2) { ErrorLog::TooManyDots(cursor_copy, num_dots); }
      RETURN_NNT("..", dots, 2);
    }
  } break;

  case '\\': {
    Cursor cursor_copy = cursor;
    size_t dist = 1;

    IncrementCursor();
    ++dist;
    switch(*cursor) {
    case '\\':
      IncrementCursor();
      RETURN_NNT("", newline, 0);
      break;
    case '\0':
      // Ignore the following newline
      IncrementCursor();
      return Next();
    case ' ':
    case '\t':
      while (IsWhitespace(*cursor)) {
        IncrementCursor();
        ++dist;
      }
      if (*cursor == '\0') {
        IncrementCursor();
        return Next();
      }

    // Intentionally falling through. Looking at a non-whitespace after a '\'
    default:
      ErrorLog::NonWhitespaceAfterNewlineEscape(cursor_copy, dist);
      return Next();
    }
  } break;

  case '#': {
    IncrementCursor();
    Cursor cursor_copy = cursor;

    if (!IsAlpha(*cursor)) {
      ErrorLog::InvalidHashtag(cursor_copy);
      return Next();
    }

    do { IncrementCursor(); } while (IsAlphaNumericOrUnderscore(*cursor));

    if (cursor.offset - cursor_copy.offset == 0) {
      ErrorLog::InvalidHashtag(cursor_copy);
    }

    char old_char       = *cursor;
    *cursor             = '\0';
    const char *tag_ref = cursor.line.ptr + cursor_copy.offset;
    size_t tag_len      = strlen(tag_ref);
    char *tag           = new char[tag_len + 1];
    strcpy(tag, tag_ref);
    *cursor             = old_char;

    RETURN_NNT(tag, hashtag, tag_len + 1);
  } break;

  case '+':
  case '%':
  case '<': 
  case '>': 
  case '|':
  case '^': {
    char first_char = *cursor;
    IncrementCursor();

    char *token = new char[3];
    token[0] = first_char;
    if (*cursor == '=') {
      IncrementCursor();
      token[1] = '=';
    } else {
      token[1] = '\0';
    }
    token[2] = '\0';

    RETURN_NNT(token, op_b, strlen(token));
  } break;

  case '*':
    IncrementCursor();
    if (*cursor == '/') {
      IncrementCursor();
      if (*cursor == '/') {
        // Looking at "*//" which should be parsed as an asterisk followed by a
        // one-line comment.
        BackUpCursor();
        RETURN_NNT("*", op_b, 1);
      } else {
        Cursor cursor_copy = cursor;
        cursor_copy.offset -= 2;
        ErrorLog::NotInMultilineComment(cursor_copy);
        return Next();
      }
    } else if (*cursor == '=') {
      IncrementCursor();
      RETURN_NNT("*=", op_b, 2);
    } else {
      RETURN_NNT("*", op_b, 1);
    }

  case '&': {
    IncrementCursor();
    if (*cursor == '=') {
      IncrementCursor();
      RETURN_NNT("&=", op_b, 2);
    } else {
      RETURN_NNT("&", op_bl, 1);
    }
  } break;

  case ':':  {
    IncrementCursor();

    if (*cursor == '=') {
      IncrementCursor();
      RETURN_NNT(":=", op_b, 2);

    } else if (*cursor == '>') {
      IncrementCursor();
      RETURN_NNT(":>", op_b, 2);

    } else {
      RETURN_NNT(":", colon, 1);
    }
  } break;

  case '!': {
    IncrementCursor();
    if (*cursor == '=') {
      IncrementCursor();
      RETURN_NNT("!=", op_b, 2);
    } else {
      RETURN_NNT("!", op_l, 1);
    }
  } break;

  case '-': {
    IncrementCursor();
    if (*cursor == '=') {
      IncrementCursor();
      RETURN_NNT("-=", op_b, 2);

    } else if (*cursor == '>') {
      IncrementCursor();
      auto nptr = new AST::TokenNode(cursor, "->");
      nptr->op = Language::Operator::Arrow;
      return NNT(nptr, Language::fn_arrow);

    } else if (*cursor == '-') {
      IncrementCursor();
      RETURN_TERMINAL(Hole, Unknown, IR::Value::None());

    } else {
      RETURN_NNT("-", op_bl, 1);
    }
  } break;

  case '=': {
    IncrementCursor();
    if (*cursor == '=') {
      IncrementCursor();
      RETURN_NNT("==", op_b, 2);

    } else if (*cursor == '>') {
      IncrementCursor();
      RETURN_NNT("=>", op_b, 2);

    } else {
      RETURN_NNT("=", eq, 1);
    }
  } break;

  case '/': {
    IncrementCursor();
    if (*cursor == '/') {
      // Ignore comments altogether
      SkipToEndOfLine();
      return Next();

    } else if (*cursor == '=') {
      IncrementCursor();
      RETURN_NNT("/=", op_b, 2);

    } else if (*cursor == '*') {
      IncrementCursor();
      char back_one = *cursor;
      IncrementCursor();

      size_t comment_layer = 1;

      while (comment_layer != 0) {
        if (ifs.eof()) {
          ErrorLog::RunawayMultilineComment();
          RETURN_NNT("", eof, 0);

        } else if (back_one == '/' && *cursor == '*') {
          ++comment_layer;

        } else if (back_one == '*' && *cursor == '/') {
          --comment_layer;
        }

        back_one = *cursor;
        IncrementCursor();
      }

      // Ignore comments altogether
      return Next();

    } else {
      RETURN_NNT("/", op_b, 1);
    }

  } break;

  case '"': {
    IncrementCursor();

    std::string str_lit = "";

    while (*cursor != '"' && *cursor != '\0') {
      if (*cursor == '\\') {
        IncrementCursor();
        switch (*cursor) {
        case '\'': {
          str_lit += '\'';
          Cursor cursor_copy = cursor;
          --cursor_copy.offset;
          ErrorLog::EscapedSingleQuoteInStringLit(cursor_copy);
        } break;

        case '\\': str_lit += '\\'; break;
        case '"':  str_lit += '"';  break;
        case 'a':  str_lit += '\a'; break;
        case 'b':  str_lit += '\b'; break;
        case 'f':  str_lit += '\f'; break;
        case 'n':  str_lit += '\n'; break;
        case 'r':  str_lit += '\r'; break;
        case 't':  str_lit += '\t'; break;
        case 'v':  str_lit += '\v'; break;

        default: {
          Cursor cursor_copy = cursor;
          --cursor_copy.offset;
          ErrorLog::InvalidEscapeCharInStringLit(cursor_copy);

          str_lit += *cursor;
        } break;
        }
      } else {
        str_lit += *cursor;
      }

      IncrementCursor();
    }

    if (*cursor == '\0') {
      ErrorLog::RunawayStringLit(cursor);
    } else {
      IncrementCursor();
    }

    // Not leaked. It's owned by a terminal which is persistent.
    char *cstr = new char[str_lit.size() + 2];
    strcpy(cstr + 1, str_lit.c_str());
    cstr[0] = '\1';
    RETURN_TERMINAL(StringLiteral, String, IR::Value(cstr));
  } break;

  case '\'': {
    IncrementCursor();
    char result;

    switch (*cursor) {
    case '\t':
      ErrorLog::TabInCharLit(cursor);
      result = '\t';
      break;

    case '\0': {
      ErrorLog::RunawayCharLit(cursor);

      RETURN_TERMINAL(Char, Char, IR::Value::Char('\0'));
    }
    case '\\': {
      IncrementCursor();
      switch (*cursor) {
      case '\"': {
        result = '"';
        Cursor cursor_copy = cursor;
        --cursor_copy.offset;
        ErrorLog::EscapedDoubleQuoteInCharLit(cursor_copy);
      } break;
      case '\\': result = '\\'; break;
      case '\'': result = '\''; break;
      case 'a': result  = '\a'; break;
      case 'b': result  = '\b'; break;
      case 'f': result  = '\f'; break;
      case 'n': result  = '\n'; break;
      case 'r': result  = '\r'; break;
      case 't': result  = '\t'; break;
      case 'v': result  = '\v'; break;
      default:
        Cursor cursor_copy = cursor;
        --cursor_copy.offset;
        ErrorLog::InvalidEscapeCharInCharLit(cursor_copy);
        result = *cursor;
      }
      break;
    }
    default: { result = *cursor; } break;
    }

    IncrementCursor();

    if (*cursor == '\'') {
      IncrementCursor();
    } else {
      ErrorLog::RunawayCharLit(cursor);
    }

    RETURN_TERMINAL(Char, Char, IR::Value::Char(result));
  } break;

  case '?':
    ErrorLog::InvalidCharQuestionMark(cursor);
    IncrementCursor();
    return Next();

  case '~':
    ErrorLog::InvalidCharTilde(cursor);
    IncrementCursor();
    return Next();

  case '_': UNREACHABLE;
  default: UNREACHABLE;
  }
}

#undef RETURN_NNT
#undef RETURN_TERMINAL
