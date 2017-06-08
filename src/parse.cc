#include "ast/ast.h"
#include "base/debug.h"
#include "base/types.h"
#include "constants_and_enums.h"
#include "error_log.h"
#include "nnt.h"
#include "util/timer.h"
#include <iostream>
#include <queue>
#include <vector>

namespace AST {
struct Node;
} // namespace AST

using NPtrVec = std::vector<AST::Node *>;

class Rule {
public:
  using OptVec = std::vector<u64>;
  using fnptr = AST::Node *(*)(NPtrVec &&);

  Rule(Language::NodeType output, const OptVec &input, fnptr fn)
      : output_(output), input_(input), fn_(fn) {}

  size_t size() const { return input_.size(); }

  bool match(const std::vector<Language::NodeType> &node_type_stack) const {
    // The stack needs to be long enough to match.
    if (input_.size() > node_type_stack.size()) return false;

    size_t stack_index = node_type_stack.size() - 1;
    size_t rule_index  = input_.size() - 1;

    // Iterate through backwards and exit as soon as you see a node whose
    // type does not match the rule.
    for (size_t i = 0; i < input_.size(); ++i, --rule_index, --stack_index) {
      if ((input_[rule_index] & node_type_stack[stack_index]) == 0) {
        return false;
      }
    }

    // If you complete the loop, there is a match.
    return true;
  }

  void apply(NPtrVec &node_stack,
             std::vector<Language::NodeType> &node_type_stack) const {
    // Make a vector for the rule function to take as input. It will begin with
    // size() shared_ptrs.
    NPtrVec nodes_to_reduce(size());

    // A rule's size cannot be empty, so the int value for i will always start
    // at
    // a non-negative integer. We use an int so we can condition on i >= 0.
    // (unsigned values always satisfy that condition).
    for (int i = (int)size() - 1; i >= 0; --i) {
      // We need an unsigned value to index nodes_to_reduce. This is why we cast
      // back to size_t.
      nodes_to_reduce[(size_t)i] = std::move(node_stack.back());

      node_type_stack.pop_back();
      node_stack.pop_back();
    }

    auto new_ptr = fn_(std::move(nodes_to_reduce));

    for (auto &ptr : nodes_to_reduce) {
      delete ptr;
      ptr = nullptr;
    }

    node_stack.push_back(std::move(new_ptr));
    node_type_stack.push_back(output_);
}

private:
  Language::NodeType output_;
  OptVec input_;
  fnptr fn_;
};


namespace debug {
extern bool parser;
} // namespace debug

extern AST::Node *BuildBinaryOperator(NPtrVec &&nodes);
extern AST::Node *BuildKWExprBlock(NPtrVec &&nodes);
extern AST::Node *BuildKWBlock(NPtrVec &&nodes);
extern AST::Node *Parenthesize(NPtrVec &&nodes);
extern AST::Node *BuildEmptyParen(NPtrVec &&nodes);
extern AST::Node *BracedStatements(NPtrVec &&nodes);
extern AST::Node *OneBracedStatement(NPtrVec &&nodes);
extern AST::Node *EmptyBraces(NPtrVec &&nodes);
extern AST::Node *BracedStatementsSameLineEnd(NPtrVec &&nodes);

template <size_t N> static AST::Node *drop_all_but(NPtrVec &&nodes) {
  auto temp = nodes[N];
  ASSERT(temp, "stolen pointer is null");
  nodes[N] = nullptr;
  return temp;
}

template <typename T> static T *steal_node(AST::Node *&n) {
  auto temp = (T *)n;
  ASSERT(temp, "stolen pointer is null");
  n = nullptr;
  return temp;
}

static AST::Node *CombineColonEq(NPtrVec &&nodes) {
  auto tk_node   = (AST::TokenNode *)nodes[0];
  tk_node->token = ":=";
  tk_node->op    = Language::Operator::ColonEq;

  return drop_all_but<0>(std::move(nodes));
}

AST::Node *EmptyFile(NPtrVec &&nodes) {
  auto stmts = new AST::Statements;
  stmts->loc = nodes[0]->loc;
  return stmts;
}

namespace ErrMsg {
template <size_t PrevIndex> AST::Node *MaybeMissingComma(NPtrVec &&nodes) {
  ErrorLog::MissingComma(nodes[PrevIndex]->loc);
  auto tk_node = new AST::TokenNode(nodes[PrevIndex]->loc, ",");
  return BuildBinaryOperator({steal_node<AST::Node>(nodes[PrevIndex]), tk_node,
                              steal_node<AST::Node>(nodes[PrevIndex + 1])});
}

template <size_t RTN, size_t RES> AST::Node *Reserved(NPtrVec &&nodes) {
  ErrorLog::Reserved(nodes[RES]->loc, ((AST::TokenNode *)nodes[RES])->token);

  return new AST::Identifier(nodes[RTN]->loc, "invalid_node");
}

template <size_t RTN, size_t RES1, size_t RES2>
AST::Node *BothReserved(NPtrVec &&nodes) {
  ErrorLog::Reserved(nodes[RES1]->loc, ((AST::TokenNode *)nodes[RES1])->token);
  ErrorLog::Reserved(nodes[RES2]->loc, ((AST::TokenNode *)nodes[RES2])->token);
  return new AST::Identifier(nodes[RTN]->loc, "invalid_node");
}

AST::Node *NonBinop(NPtrVec &&nodes) {
  ErrorLog::NotBinary(nodes[1]->loc, ((AST::TokenNode *)nodes[1])->token);
  return new AST::Identifier(nodes[1]->loc, "invalid_node");
}

template <size_t RTN, size_t RES> AST::Node *NonBinopReserved(NPtrVec &&nodes) {
  ErrorLog::NotBinary(nodes[1]->loc, ((AST::TokenNode *)nodes[1])->token);
  ErrorLog::Reserved(nodes[RES]->loc, ((AST::TokenNode *)nodes[RES])->token);
  return new AST::Identifier(nodes[RTN]->loc, "invalid_node");
}

AST::Node *NonBinopBothReserved(NPtrVec &&nodes) {
  ErrorLog::Reserved(nodes[0]->loc, ((AST::TokenNode *)nodes[0])->token);
  ErrorLog::NotBinary(nodes[1]->loc, ((AST::TokenNode *)nodes[1])->token);
  ErrorLog::Reserved(nodes[2]->loc, ((AST::TokenNode *)nodes[2])->token);
  return new AST::Identifier(nodes[1]->loc, "invalid_node");
}
} // namespace ErrMsg
namespace Language {
static constexpr u64 OP_B  = op_b | comma | dots | colon | eq;
static constexpr u64 OP_LT = op_lt | kw_else;
static constexpr u64 EXPR  = expr | fn_expr | kw_else;
// Used in error productions only!
static constexpr u64 RESERVED =
    kw_expr_block | kw_else | kw_block | kw_struct | op_lt;

// Here are the definitions for all rules in the langugae. For a rule to be
// applied, the node types on the top of the stack must match those given in the
// list (second line of each rule). If so, then the function given in the third
// line of each rule is applied, replacing the matched nodes. Lastly, the new
// nodes type is set to the given type in the first line.
auto Rules = std::vector<Rule>{
    Rule(fn_expr, {EXPR, fn_arrow, EXPR}, BuildBinaryOperator),
    Rule(expr, {EXPR, (op_bl | OP_B), EXPR}, BuildBinaryOperator),
    Rule(op_b, {colon, eq}, CombineColonEq),
    Rule(fn_expr, {EXPR, fn_arrow, RESERVED}, ErrMsg::Reserved<1, 2>),
    Rule(fn_expr, {RESERVED, fn_arrow, EXPR}, ErrMsg::Reserved<1, 0>),
    Rule(fn_expr, {RESERVED, fn_arrow, RESERVED},
         ErrMsg::BothReserved<1, 0, 2>),
    Rule(expr, {EXPR, (OP_B | op_bl | dots), RESERVED}, ErrMsg::Reserved<1, 2>),
    Rule(expr, {RESERVED, (OP_B | op_bl), RESERVED},
         ErrMsg::BothReserved<1, 0, 2>),
    Rule(expr, {EXPR, op_l, RESERVED}, ErrMsg::NonBinopReserved<1, 2>),
    Rule(expr, {RESERVED, op_l, RESERVED}, ErrMsg::NonBinopBothReserved),
    Rule(expr, {EXPR, l_paren, EXPR, r_paren}, AST::Binop::BuildCallOperator),
    Rule(expr, {EXPR, l_paren, r_paren}, BuildEmptyParen),
    Rule(expr, {EXPR, l_bracket, EXPR, r_bracket},
         AST::Binop::BuildIndexOperator),
    Rule(expr, {l_bracket, r_bracket}, AST::ArrayLiteral::BuildEmpty),
    Rule(expr, {EXPR, hashtag}, AST::Expression::AddHashtag),
    Rule(expr, {l_paren, EXPR, EXPR, r_paren}, ErrMsg::MaybeMissingComma<1>),
    Rule(expr, {l_bracket, EXPR, EXPR, r_bracket},
         ErrMsg::MaybeMissingComma<1>),
    Rule(expr, {EXPR, l_bracket, EXPR, EXPR, r_bracket},
         ErrMsg::MaybeMissingComma<2>),
    Rule(expr, {l_bracket, EXPR, semicolon, EXPR, r_bracket},
         AST::ArrayType::build),
    Rule(expr, {l_bracket, EXPR, semicolon, RESERVED, r_bracket},
         ErrMsg::Reserved<0, 3>),
    Rule(expr, {l_bracket, RESERVED, semicolon, EXPR, r_bracket},
         ErrMsg::Reserved<0, 1>),
    Rule(expr, {l_bracket, RESERVED, semicolon, RESERVED, r_bracket},
         ErrMsg::BothReserved<0, 1, 3>),
    Rule(bof, {bof, newline}, drop_all_but<0>),
    Rule(eof, {newline, eof}, drop_all_but<1>),
    Rule(prog, {bof, eof}, EmptyFile),
    Rule(prog, {bof, stmts, eof}, drop_all_but<1>),
    Rule(r_paren, {newline, r_paren}, drop_all_but<1>),
    Rule(r_bracket, {newline, r_bracket}, drop_all_but<1>),
    Rule(r_brace, {newline, r_brace}, drop_all_but<1>),
    Rule(r_double_brace, {newline, r_double_brace}, drop_all_but<1>),
    Rule(l_brace, {newline, l_brace}, drop_all_but<1>),
    Rule(l_double_brace, {newline, l_double_brace}, drop_all_but<1>),
    Rule(stmts, {newline, stmts}, drop_all_but<1>),
    Rule(r_paren, {r_paren, newline}, drop_all_but<0>),
    Rule(r_bracket, {r_bracket, newline}, drop_all_but<0>),
    Rule(r_brace, {r_brace, newline}, drop_all_but<0>),
    Rule(r_double_brace, {r_double_brace, newline}, drop_all_but<0>),
    Rule(braced_stmts, {l_brace, stmts, stmts | expr | fn_expr, r_brace},
         BracedStatementsSameLineEnd),
    Rule(braced_stmts, {l_brace, stmts, r_brace}, BracedStatements),
    Rule(braced_stmts, {l_brace, r_brace}, EmptyBraces),
    Rule(braced_stmts, {l_brace, (expr | fn_expr), r_brace},
         OneBracedStatement),
    Rule(expr, {l_double_brace, stmts, stmts | expr | fn_expr, r_double_brace},
         AST::CodeBlock::BuildFromStatementsSameLineEnd),
    Rule(expr, {l_double_brace, stmts, r_double_brace},
         AST::CodeBlock::BuildFromStatements),
    Rule(expr, {l_double_brace, r_double_brace}, AST::CodeBlock::BuildEmpty),
    Rule(expr, {l_double_brace, (expr | fn_expr), r_double_brace},
         AST::CodeBlock::BuildFromOneStatement),
    Rule(expr, {fn_expr, braced_stmts}, AST::FunctionLiteral::build),

    // Call and index operator with reserved words. We can't put reserved words
    // in the first slot because that might conflict with a real use case. For
    // example, "if(a)".
    Rule(expr, {EXPR, l_paren, RESERVED, r_paren}, ErrMsg::Reserved<0, 2>),
    Rule(expr, {EXPR, l_bracket, RESERVED, r_bracket}, ErrMsg::Reserved<0, 2>),

    Rule(expr, {(op_l | op_bl | OP_LT), EXPR}, AST::Unop::BuildLeft),
    Rule(expr, {RESERVED, (OP_B | op_bl), EXPR}, ErrMsg::Reserved<1, 0>),
    Rule(expr, {EXPR, dots}, AST::Unop::BuildDots),
    Rule(expr, {l_paren | l_ref, EXPR, r_paren}, Parenthesize),
    Rule(expr, {l_bracket, EXPR, r_bracket}, AST::ArrayLiteral::build),
    Rule(expr, {l_paren, RESERVED, r_paren}, ErrMsg::Reserved<1, 1>),
    Rule(expr, {l_bracket, RESERVED, r_bracket}, ErrMsg::Reserved<1, 1>),
    Rule(stmts, {stmts, (expr | fn_expr | stmts), newline},
         AST::Statements::build_more),
    Rule(expr, {(kw_block | kw_struct), braced_stmts}, BuildKWBlock),

    Rule(expr, {RESERVED, dots}, ErrMsg::Reserved<1, 0>),
    Rule(expr, {(op_l | op_bl | OP_LT), RESERVED}, ErrMsg::Reserved<0, 1>),
    Rule(expr, {RESERVED, op_l, EXPR}, ErrMsg::NonBinopReserved<1, 0>),
    Rule(stmts, {(expr | fn_expr | kw_else), (newline | eof)},
         AST::Statements::build_one),
    Rule(comma, {comma, newline}, drop_all_but<0>),
    Rule(l_paren, {l_paren, newline}, drop_all_but<0>),
    Rule(l_bracket, {l_bracket, newline}, drop_all_but<0>),
    Rule(l_brace, {l_brace, newline}, drop_all_but<0>),
    Rule(l_double_brace, {l_double_brace, newline}, drop_all_but<0>),
    Rule(stmts, {stmts, newline}, drop_all_but<0>),
    Rule(stmts, {kw_expr_block, EXPR, braced_stmts}, BuildKWExprBlock),
    Rule(expr, {kw_struct, EXPR, braced_stmts}, BuildKWExprBlock),

    Rule(expr, {EXPR, op_l, EXPR}, ErrMsg::NonBinop),
    Rule(stmts, {op_lt}, AST::Jump::build),
    Rule(expr, {EXPR, EXPR, braced_stmts}, AST::ScopeNode::Build),

    Rule(expr, {EXPR, braced_stmts}, AST::ScopeNode::BuildVoid),
};
} // namespace Language

extern NNT NextToken(Cursor &cursor); // Defined in Lexer.cpp

enum class ShiftState : char { NeedMore, EndOfExpr, MustReduce };
std::ostream& operator<<(std::ostream& os, ShiftState s) {
  switch (s) {
  case ShiftState::NeedMore:
    return os << "NeedMore";
  case ShiftState::EndOfExpr:
    return os << "EndOfExpr";
  case ShiftState::MustReduce:
    return os << "MustReduce";
  default:
    UNREACHABLE;
  }
}

struct ParseState {
  ParseState(const Cursor &c) : lookahead_(nullptr, Language::bof) {
    lookahead_.node.reset(new AST::TokenNode(c));
  }

  template <size_t N> inline Language::NodeType get_type() const {
    return node_type_stack_[node_type_stack_.size() - N];
  }

  template <size_t N> inline AST::Node *get() const {
    return node_stack_[node_stack_.size() - N];
  }

  ShiftState shift_state() const {
    using namespace Language;
    // If the size is just 1, no rule will match so don't bother checking.
    if (node_stack_.size() < 2) { return ShiftState::NeedMore; }

    if (lookahead_.node_type == newline) {
      // TODO it's much more complicated than this. (braces?)
      return brace_count == 0 ? ShiftState::EndOfExpr : ShiftState::MustReduce;
    }

    if (get_type<1>() == dots) {
      return (lookahead_.node_type &
              (op_bl | op_l | op_lt | expr | fn_expr | l_paren | l_bracket))
                 ? ShiftState::NeedMore
                 : ShiftState::MustReduce;
    }

    if (lookahead_.node_type == l_brace && get_type<1>() == fn_expr &&
        get_type<2>() == fn_arrow) {
      return ShiftState::MustReduce;
    }

    if (lookahead_.node_type == l_brace &&
        (get_type<1>() & (fn_expr | kw_struct | kw_block))) {
      return ShiftState::NeedMore;
    }

    if (get_type<1>() == newline && get_type<2>() == comma) {
      return ShiftState::MustReduce;
    }

    // We require struct params to be in parentheses.
    if (lookahead_.node_type == l_paren && get_type<1>() == kw_struct) {
      return ShiftState::NeedMore;
    }

    if (get_type<1>() == op_lt && lookahead_.node_type != newline) {
      return ShiftState::NeedMore;
    }

    if (get_type<1>() == kw_block && lookahead_.node_type == newline) {
      return ShiftState::NeedMore;
    }

    if (get_type<2>() == kw_block && get_type<1>() == newline) {
      return ShiftState::NeedMore;
    }

    if (node_stack_.size() > 2 && get_type<3>() == kw_expr_block &&
        get_type<2>() == expr && get_type<1>() == newline) {
      return ShiftState::NeedMore;
    }

    if (lookahead_.node_type == r_paren) {
      return ShiftState::MustReduce;
    }

    if (get_type<2>() & OP_) {
      auto left_prec = precedence(((AST::TokenNode *)get<2>())->op);
      size_t right_prec;
      if (lookahead_.node_type & OP_) {
        right_prec = precedence(
            static_cast<AST::TokenNode *>(lookahead_.node.get())->op);
      } else if (lookahead_.node_type == l_paren) {
        right_prec = precedence(Operator::Call);
      } else {
        return ShiftState::MustReduce;
      }

      return (left_prec < right_prec) ||
                     (left_prec == right_prec &&
                      (left_prec & assoc_mask) == right_assoc)
                 ? ShiftState::NeedMore
                 : ShiftState::MustReduce;
    }
    return ShiftState::MustReduce;
  }


  std::vector<Language::NodeType> node_type_stack_;
  NPtrVec node_stack_;
  NNT lookahead_;

  // We actually don't care about mathing braces. That is, we can count {[) as 1
  // open, because we are only using this to determine for the REPL if we should
  // prompt for further input. If it's wrong, we won't be able to to parse
  // anyway, so it only needs to be the correct value when the braces match.
  int brace_count = 0;
};

// Print out the debug information for the parse stack, and pause.
static void Debug(ParseState *ps, Cursor* cursor = nullptr) {
  // Clear the screen
  fprintf(stderr, "\033[2J\033[1;1H\n");
  if (cursor) {
    fprintf(stderr, "%s", cursor->line.c_str());
    fprintf(stderr, "%*s^\n(offset = %lu)\n\n",
            static_cast<int>(cursor->offset), "", cursor->offset);
  }
  for (auto x : ps->node_type_stack_) { fprintf(stderr, "%lu, ", x); }
  fprintf(stderr, " -> %lu", ps->lookahead_.node_type);
  fputs("", stderr);

  for (const auto &node_ptr : ps->node_stack_) {
    fputs(node_ptr->to_string(0).c_str(), stderr);
  }
  fgetc(stdin);
}

static void Shift(ParseState *ps, Cursor *c) {
  ps->node_type_stack_.push_back(ps->lookahead_.node_type);
  ps->node_stack_.push_back(ps->lookahead_.node.release());
  ps->lookahead_ = NextToken(*c);
  if (ps->lookahead_.node_type & (Language::l_paren | Language::l_bracket |
                              Language::l_brace | Language::l_double_brace)) {
    ++ps->brace_count;
  } else if (ps->lookahead_.node_type &
             (Language::r_paren | Language::r_bracket | Language::r_brace |
              Language::r_double_brace)) {
    --ps->brace_count;
  }
}

static bool Reduce(ParseState *ps) {
  const Rule *matched_rule_ptr = nullptr;
  for (const Rule &rule : Language::Rules) {
    if (rule.match(ps->node_type_stack_)) {
      matched_rule_ptr = &rule;
      break;
    }
  }

  // If you make it to the end of the rules and still haven't matched, then
  // return false
  if (matched_rule_ptr == nullptr) { return false; }

  matched_rule_ptr->apply(ps->node_stack_, ps->node_type_stack_);

  return true;
}

void CleanUpReduction(ParseState* state, Cursor* cursor) {
  // Reduce what you can
  while (Reduce(state)) {
    if (debug::parser) { Debug(state, cursor); }
  }

  state->node_type_stack_.push_back(Language::eof);
  state->node_stack_.push_back(new AST::TokenNode(*cursor, ""));
  state->lookahead_ =
      NNT(std::make_unique<AST::TokenNode>(*cursor, ""), Language::eof);

  // Reduce what you can again
  while (Reduce(state)) {
    if (debug::parser) { Debug(state, cursor); }
  }
  if (debug::parser) { Debug(state, cursor); }
}

AST::Statements *Repl::Parse() {
  first_entry = true; // Show '>> ' the first time.

  Cursor cursor;
  cursor.source_file = this;

  auto state = ParseState(cursor);
  Shift(&state, &cursor);

  while (true) {
    auto shift_state = state.shift_state();
    switch (shift_state) {
    case ShiftState::NeedMore:
      Shift(&state, &cursor);

      if (debug::parser) {
        Debug(&state, &cursor);
      }
      continue;
    case ShiftState::EndOfExpr:
      CleanUpReduction(&state, &cursor);
      return (AST::Statements *)state.node_stack_.back();
    case ShiftState::MustReduce:
      Reduce(&state) || (Shift(&state, &cursor), true);
      if (debug::parser) { Debug(&state, &cursor); }
    }
  }
}

AST::Statements *File::Parse() {
  Cursor cursor ;
  cursor.source_file = this;

  auto state = ParseState(cursor);
  Shift(&state, &cursor);

  while (state.lookahead_.node_type != Language::eof) {
    ASSERT(state.node_type_stack_.size() == state.node_stack_.size(), "");
    // Shift if you are supposed to, or if you are unable to reduce.
    if (state.shift_state() == ShiftState::NeedMore || !Reduce(&state)) {
      Shift(&state, &cursor);
    }

    if (debug::parser) { Debug(&state); }
  }

  // Cleanup
  CleanUpReduction(&state, &cursor);

  // Finish
  if (state.node_stack_.size() > 1) {
    std::vector<Cursor> lines;

    size_t last_chosen_line = 0;
    for (size_t i = 0; i < state.node_stack_.size(); ++i) {
      if (state.node_stack_[i]->loc.line_num == last_chosen_line) { continue; }
      if (state.node_type_stack_[i] &
          (Language::braced_stmts | Language::l_paren | Language::r_paren |
           Language::l_bracket | Language::r_bracket | Language::l_brace |
           Language::r_brace | Language::semicolon | Language::hashtag |
           Language::fn_arrow | Language::expr)) {
        lines.push_back(state.node_stack_[i]->loc);
        last_chosen_line = state.node_stack_[i]->loc.line_num;
      }
    }
    ErrorLog::UnknownParserError(name, lines);
  }

  return (AST::Statements *)state.node_stack_.back();
}

extern Timer timer;
std::map<std::string, File *> source_map;
std::vector<AST::Statements *>
ParseAllFiles(std::queue<std::string> file_names) {
  std::vector<AST::Statements *> stmts;
  while (!file_names.empty()) {
    std::string file_name = file_names.front();
    file_names.pop();

    if (source_map.find(file_name) != source_map.end()) { continue; }

    RUN(timer, "Parsing a file") {
      auto source_file      = new File(file_name);
      source_map[file_name] = source_file;
      stmts.push_back(source_file->Parse());
    }
  }
  return stmts;
}
