#ifndef ICARUS_TYPEDEFS_H
#define ICARUS_TYPEDEFS_H

#include <vector>
#include <memory>

namespace AST {
  class Node;
  class Expression;
  class Identifier;
  class Declaration;
  class Statements;
}

using NPtr = std::shared_ptr<AST::Node>;
using EPtr = std::shared_ptr<AST::Expression>;
using IdPtr = std::shared_ptr<AST::Identifier>;
using DeclPtr = std::shared_ptr<AST::Declaration>;
using StmtsPtr = std::shared_ptr<AST::Statements>;

using NPtrVec = std::vector<NPtr>;

#endif  // ICARUS_TYPEDEFS_H
