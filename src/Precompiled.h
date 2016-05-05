#ifndef ICARUS_PRECOMPILED_H
#define ICARUS_PRECOMPILED_H

// Common standard headers
#include <iostream>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <queue>
#include <stack>
#include <sstream>
#include <fstream>

// Each time a file is imported, it will be added to the queue. We then parse
// each file off the queue until the queue is empty. We avoid circular calls by
// checking if the map is already filled before parsing.
extern std::queue<std::string> file_queue;

#include "ConstantsAndEnums.h"

namespace llvm {
class Value;
class Function;
} // namespace llvm

namespace AST {
struct Node;
struct Expression;
struct Identifier;
struct TokenNode;
struct Declaration;
struct Binop;
struct Statements;
struct FunctionLiteral;
struct EnumLiteral;
struct StructLiteral;
} // namespace AST

struct Type;
struct Primitive;
struct Array;
struct Tuple;
struct Pointer;
struct Function;
struct Enumeration;
struct Structure;
struct ParametricStructure;
struct DependentType;
struct TypeVariable;
struct QuantumType;
struct RangeType;

struct Scope;
struct BlockScope;
struct FnScope;

using NPtrVec = std::vector<AST::Node *>;

struct TokenLocation {
  std::string file;
  size_t line_num;
  char offset;

  TokenLocation() : file(""), line_num(0), offset(0){};
};

#include "Context.h"
#include "ErrorLog.h"

#include "DependencyFunctions.h"

namespace Language {
// Using masks to make determination node types easier. Starting masks in the
// 8th bit, leaves bits 0-7 for standard enumeration. This is safe because we
// will never have more than 128 NodeTypes in a given section.

constexpr int OP_        = 1 << 8;
constexpr int BIN_       = 1 << 9;
constexpr int CHAIN_     = 1 << 10;
constexpr int LEFT_      = 1 << 11;
constexpr int EXPR_      = 1 << 12;
constexpr int COMPOSITE_ = 1 << 13;

enum NodeType : short {
  // Generic
  unknown,
  keep_current,
  bof,
  eof,
  newline,
  comment,

  semicolon,
  hashtag,

  // Encapsulators
  left_paren,
  right_paren,
  left_brace,
  right_brace,
  left_bracket,
  right_bracket,

  identifier,

  reserved_break,
  reserved_if,
  reserved_else,
  reserved_case,
  reserved_for,
  reserved_enum,
  reserved_while,
  reserved_continue,
  reserved_string,
  reserved_struct,
  reserved_repeat,
  reserved_restart,

  binop = BIN_ + OP_,
  tick,
  dot,
  declop,
  fn_arrow,
  rocket_operator,
  assign_operator,
  reserved_in,

  chainop = CHAIN_ + OP_,

  not_operator = LEFT_ + OP_,
  dereference,
  reserved_free,
  reserved_print,
  reserved_import,

  indirection = LEFT_ + BIN_ + OP_,
  dots,
  negation,

  reserved_return = EXPR_ + LEFT_ + OP_,

  expression = COMPOSITE_ + EXPR_,
  fn_expression,
  fn_literal,

  declaration = COMPOSITE_,
  if_stmt,
  if_else_stmt,
  while_stmt,
  statements,
  program
};

inline bool is_operator(NodeType t) { return (t & OP_) != 0; }
} // namespace Language

#include "AST.h"

#endif // ICARUS_PRECOMPILED_H