#include "AST.h"

namespace AST {
  llvm::Value* Identifier::generate_code(Scope* scope) {
    // FIXME
    return builder.CreateLoad(val_, token().c_str());
  }

  llvm::Value* Terminal::generate_code(Scope* scope) {
    // TODO Do I want to use string-to-X functions, or should I roll my own?
    //
    // The benefits are clear, but this ties me to using the same representation
    // that C++ uses.

    if (expr_type_ == Type::Unknown || expr_type_ == Type::TypeError) {
      return nullptr;

    } else if (expr_type_ == Type::Bool) {
      // A bool is an unsigned 1-bit integer
      return llvm::ConstantInt::get(llvm::getGlobalContext(),
          llvm::APInt(1, token() == "true" ? 1 : 0, false));

    } else if (expr_type_ == Type::Char) {
      // A character is an unsigend 8-bit integer
      return llvm::ConstantInt::get(llvm::getGlobalContext(),
          llvm::APInt(8, static_cast<unsigned int>(token()[0]), false));

    } else if (expr_type_ == Type::Int) {
      // An int is a 64-bit signed integer
      return llvm::ConstantInt::get(llvm::getGlobalContext(),
          llvm::APInt(64, std::stoul(token()), true));
 
    } else if (expr_type_ == Type::Real) {
      return llvm::ConstantFP::get(llvm::getGlobalContext(),
          llvm::APFloat(std::stod(token())));

    } else if (expr_type_ == Type::String) {
      // TODO String should not be a primitive type
      return nullptr; 

    } else if (expr_type_ == Type::Type_) {
      return nullptr;

    } else if (expr_type_ == Type::UInt) {
      // A uint is a 64-bit unsigned integer
      return llvm::ConstantInt::get(llvm::getGlobalContext(),
          llvm::APInt(64, std::stoul(token()), false));
    } else {
      std::cerr << "FATAL: Terminal type is not a primitive type" << std::endl;
      return nullptr;
    }
  }

  llvm::Value* Unop::generate_code(Scope* scope) {
    llvm::Value* val = expr_->generate_code(scope);

    if (is_return()) {
      builder.CreateRet(val);
    }
    return val;
  }

  llvm::Value* Binop::generate_code(Scope* scope) {
    llvm::Value* lhs_val = lhs_->generate_code(scope);
    llvm::Value* rhs_val = rhs_->generate_code(scope);

    if (lhs_val == nullptr || rhs_val == nullptr) {
      return nullptr;
    }

    if (expr_type_ == Type::Int) {
      if (token() == "+") {
      } else if (token() == "-") {
      } else if (token() == "*") {
      } else if (token() == "/") {
      }
    } else if (expr_type_ == Type::Real) {
      if (token() == "+") {
        return builder.CreateFAdd(lhs_val, rhs_val, "addtmp");
        
      } else if (token() == "-") {
        return builder.CreateFSub(lhs_val, rhs_val, "subtmp");

      } else if (token() == "*") {
        return builder.CreateFMul(lhs_val, rhs_val, "multmp");

      }

    } else if (expr_type_ == Type::UInt) {
    }

    return nullptr;
  }


  llvm::Value* Statements::generate_code(Scope* scope) {
    for (auto& stmt : statements_) {
      stmt->generate_code(scope);
    }
    return nullptr;
  }

  llvm::Value* ChainOp::generate_code(Scope* scope) {
    return nullptr;
  }

  llvm::Value* FunctionLiteral::generate_code(Scope* scope) {
    // TODO: do this for real.
    // This is just to see if we can get it working

    if (expr_type_ == Type::Function(Type::Real, Type::Real)) {
      llvm::FunctionType *fn_type = llvm::FunctionType::get(
          llvm::Type::getDoubleTy(llvm::getGlobalContext()),
          // vector with 1 double type
          { llvm::Type::getDoubleTy(llvm::getGlobalContext()) },
          false);


      // TODO: pick a name for this anonymous function
      llvm::Function* fn = llvm::Function::Create(
          fn_type, llvm::Function::InternalLinkage, "__anon_fn", nullptr);

      auto iter = inputs_.begin();
      for (auto& arg : fn->args()) {
        arg.setName((*iter)->identifier_string());
        ++iter;
      }


      // TODO Is this possible, check with LLVM
      if (fn == nullptr) return nullptr;

      // Create a new basic block to start insertion into.
      fn_scope_->block_ = llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", fn);
      builder.SetInsertPoint(fn_scope_->block_);

      // TODO declare the correct type (currently always a real)
      // TODO verify that args are iterated over in the order they appear.
      iter = inputs_.begin();
      for (auto& arg : fn->args()) {
        (*iter)->declared_identifier()->val_ = AST::builder.CreateAlloca(
            llvm::Type::getDoubleTy(llvm::getGlobalContext()),
            nullptr,
            arg.getName());


        AST::builder.CreateStore(&arg, (*iter)->declared_identifier()->val_);
        ++iter;
      }

      // FIXME this is the wrong thing to iterate over because it includes the
      // inputs
      for (const auto& decl_ptr : fn_scope_->ordered_decls_) {
        auto id_str = decl_ptr->identifier_string();

        // There are so few inputs in the general case that this is likely more
        // efficient than a map
        // TODO verify this.
        bool found_in_inputs = false;
        for (const auto& input_decl : inputs_) {
          if (id_str == input_decl->identifier_string()){
            found_in_inputs = true;
            break;
          }
        }

        if (found_in_inputs) continue;

        decl_ptr->declared_identifier()->val_ = AST::builder.CreateAlloca(
            llvm::Type::getDoubleTy(llvm::getGlobalContext()),
            nullptr,
            id_str.c_str());
      }

      statements_->generate_code(fn_scope_);

      std::cout
        << "========================================"
        << "========================================" << std::endl;
      fn->dump();
      std::cout
        << "========================================"
        << "========================================" << std::endl;

      return fn;
    }

    return nullptr;
  }


  llvm::Value* Assignment::generate_code(Scope* scope) {
    if (rhs_->expr_type_ == Type::Real) {
      llvm::Value* val = rhs_->generate_code(scope);

      if (val == nullptr) return nullptr;

      // Look up the name.
      llvm::Value* var = scope->identifier(lhs_->token())->val_;
      if (var == nullptr) {
        return nullptr;
      }

      builder.CreateStore(val, var);
      return val;
    }

    return nullptr;
  }

  llvm::Value* Declaration::generate_code(Scope* scope) {
    return nullptr;
  }

  llvm::Value* Case::generate_code(Scope* scope) {
    return nullptr;
  }
}  // namespace AST
