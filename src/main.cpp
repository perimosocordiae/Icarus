#include <iostream>
#include <fstream>
#include <string>
#include <queue>

#include "Parser.h"
#include "AST.h"
#include "Type.h"
#include "Scope.h"
#include "ErrorLog.h"
#include "DependencySystem.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/raw_os_ostream.h"

extern llvm::Module *global_module;
extern llvm::DataLayout *data_layout;

namespace TypeSystem {
extern void GenerateLLVM();
} // namespace TypeSystem

namespace data {
extern llvm::ConstantInt *const_uint(size_t n);
} // namespace data

extern ErrorLog error_log;
extern llvm::IRBuilder<> builder;

namespace debug {
extern bool parser;
extern bool dependency_graph;
extern bool parametric_struct;
} // namespace debug

// The keys in this map represent the file names, and the values represent the
// syntax trees from the parsed file.
//
// TODO This is NOT threadsafe! If someone edits the map, it may rebalance and
// a datarace will corrupt the memory. When we start threading, we need to lock
// the map before usage.
extern std::map<std::string, AST::Statements *> ast_map;

// Each time a file is imported, it will be added to the queue. We then parse
// each file off the queue until the queue is empty. We avoid circular calls by
// checking if the map is already filled before parsing.
extern std::queue<std::string> file_queue;


// This is an enum so we can give meaningful names for error codes. However, at
// the end of the day, we must return ints. Thus, we need to use the implicit
// cast from enum to int, and so we cannot get the added type safety of an enum
// class.
namespace error_code {
enum {
  success = 0, // returning 0 denotes succes

  cyclic_dependency,
  file_does_not_exist,
  invalid_arguments,
  parse_error,
  shadow_or_type_error,
  undeclared_identifier
};
} // namespace error_code

// If the file name does not have an extension, add ".ic" to the end of it.
//
// TODO this is extremely not robust and probably has system dependencies.
std::string canonicalize_file_name(const std::string &filename) {
  auto found_dot = filename.find('.');
  return (found_dot == std::string::npos) ? filename + ".ic" : filename;
}

int main(int argc, char *argv[]) {
  // This includes naming all basic types, so it must be done even before
  // lexing.
  TypeSystem::initialize();

  // Initialize the global scope
  Scope::Global  = new BlockScope(ScopeType::Global);
  builder.SetInsertPoint(Scope::Global->entry);

  int arg_num    = 1;  // iterator over argv
  int file_index = -1; // Index of where file name is in argv
  while (arg_num < argc) {
    auto arg = argv[arg_num];

    if (strcmp(arg, "-P") == 0 || strcmp(arg, "-p") == 0) {
      debug::parser = true;

    } else if (strcmp(arg, "-D") == 0 || strcmp(arg, "-d") == 0) {
      debug::dependency_graph = true;

    } else if (strcmp(arg, "-S") == 0 || strcmp(arg, "-s") == 0) {
      debug::parametric_struct = true;

    } else if (file_index == -1) {
      // If we haven't seen a file yet, point to it
      file_index = arg_num;

    } else {
      // If we have found a file already, error out.
      std::cerr << "Provide exactly one file name." << std::endl;
      return error_code::invalid_arguments;
    }

    ++arg_num;
  }

  // Add the file to the queue
  file_queue.emplace(argv[file_index]);

  while (!file_queue.empty()) {
    std::string file_name = canonicalize_file_name(file_queue.front());
    file_queue.pop();
    auto iter = ast_map.find(file_name);

    // If we've already parsed this file, don't parse it again.
    if (iter != ast_map.end()) continue;

    // Check if file exists
    std::ifstream infile(file_name);
    if (!infile.good()) {
      // TODO do this with the error log
      std::cerr
        << "File '" << file_name << "' does not exist or cannot be accessed."
        << std::endl;
    }
    error_log.set_file(file_name);

    Parser parser(file_name);
    ast_map[file_name] = static_cast<AST::Statements *>(parser.parse());
  }

  if (error_log.num_errors() != 0) {
    std::cout << error_log;
    return error_code::parse_error;
  }



  // Init global module, function, etc.
  global_module = new llvm::Module("global_module", llvm::getGlobalContext());
  data_layout = new llvm::DataLayout(global_module);

  // TODO write the language rules to guarantee that the parser produces a
  // Statements node at top level.
    

  // Combine all statement nodes from separately-parsed files.
  auto global_statements = new AST::Statements;

  // Reserve enough space for all of them to avoid unneeded copies
  size_t num_statements = 0;
  for (const auto& kv : ast_map) {
    num_statements += kv.second->size();
  }
  global_statements->reserve(num_statements);

  for (auto& kv : ast_map) {
    global_statements->add_nodes(kv.second);
  }


  // COMPILATION STEP:
  //
  // Determine which declarations go in which scopes. Store that information
  // with the scopes. Note that assign_scope cannot possibly generate
  // compilation errors, so we don't check for them here.
  Scope::Stack.push(Scope::Global);
  global_statements->assign_scope();

  // COMPILATION STEP:
  //
  // Join the identifiers turning the syntax tree into a syntax DAG. This must
  // happen after the declarations are assigned to each scope so we have a
  // specific identifier to point to that is easy to find. This can generate an
  // undeclared identifier error.
  global_statements->join_identifiers();
  Scope::Stack.pop();

  if (error_log.num_errors() != 0) {
    std::cout << error_log;
    return error_code::undeclared_identifier;
  }

  // COMPILATION STEP:
  //
  // For each identifier, figure out which other identifiers are needed in
  // order to declare this one. This cannot generate compilation errors.
  Dependency::record(global_statements);
  // To assign type orders, we traverse the dependency graph looking for a
  // valid ordering in which we can determine the types of the nodes. This can
  // generate compilation errors if no valid ordering exists.
  Dependency::assign_order();
  TypeSystem::GenerateLLVM();

  if (error_log.num_errors() != 0) {
    std::cout << error_log;
    return error_code::cyclic_dependency;
  }

  // COMPILATION STEP:
  //
  // The name should be self-explanatory. This function looks through the
  // scope tree for a node and its ancestor with declared identifiers of the
  // same name. We do not allow shadowing of any kind whatsoever. Errors are
  // generated if shadows are encountered.
  Scope::Scope::verify_no_shadowing();
  if (error_log.num_errors() != 0) {
    std::cout << error_log;
    return error_code::shadow_or_type_error;
  }

  global_statements->determine_time();

  // Program has been verified. We can now proceed with code generation.
  // Initialize the global scope.

  { // Initialize Global scope
    for (auto decl_ptr : Scope::Global->ordered_decls_) {
      auto decl_id = decl_ptr->identifier;
      if (decl_id->is_arg) continue;

      auto decl_type = decl_id->type;
      if (decl_type.get->llvm_type == nullptr) continue;

      if (decl_type.is_function()) {
        if (decl_id->token()[0] != '_') { // Ignore operators
          auto fn_type = static_cast<Function *>(decl_type.get);
          auto mangled_name = Mangle(fn_type, decl_ptr->identifier->token());

          decl_id->alloc = decl_type.get->allocate();
          decl_id->alloc->setName(mangled_name);
        }
      } else {
        assert(decl_type == Type_ && "Global variables not currently allowed.");
      }
    }
  }


  global_statements->lrvalue_check();
  if (error_log.num_errors() != 0) {
    std::cout << error_log;
    return error_code::shadow_or_type_error;
  }


  // Generate LLVM intermediate representation.
  Scope::Stack.push(Scope::Global);
  global_statements->generate_code();
  Scope::Stack.pop();

  // TODO Optimization.

  {
    // In this anonymous scope we write the LLVM IR to a file. The point
    // of the anonymous scope is to ensure that the file is written and
    // closed before we make system calls on it (e.g., for linking).
    std::ofstream output_file_stream("ir.ll");
    llvm::raw_os_ostream output_file(output_file_stream);
    global_module->print(output_file, nullptr);
  }
  

  std::string input_file_name(argv[1]);
  std::string link_string = "gcc ir.o -o bin/";
  size_t start = input_file_name.find('/', 0) + 1;
  size_t end = input_file_name.find('.', 0);
  link_string += input_file_name.substr(start, end - start);

  system("llc -filetype=obj ir.ll");
  system(link_string.c_str());
  system("rm ir.o");

  return error_code::success;
}
