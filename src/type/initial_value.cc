#include "type.h"
#include "../ast/ast.h"

IR::Val Primitive::EmitInitialValue() const {
  switch (type_) {
  case PrimType::Err: UNREACHABLE;
  case PrimType::Unknown: UNREACHABLE;
  case PrimType::Type: return IR::Val::Type(Void);
  case PrimType::Void: UNREACHABLE;
  case PrimType::NullPtr: UNREACHABLE;
  case PrimType::Code: UNREACHABLE;
  case PrimType::Bool: return IR::Val::Bool(false);
  case PrimType::Char: return IR::Val::Char('\0');
  case PrimType::Int: return IR::Val::Int(0l);
  case PrimType::Real: return IR::Val::Real(0.0);
  case PrimType::Uint: return IR::Val::Uint(0ul);
  case PrimType::String: NOT_YET; //return IR::Val(const_cast<char *>("\0"));
  default: UNREACHABLE;
  }
}

// TODO ugly const_cast
IR::Val Pointer::EmitInitialValue() const {
  return IR::Val::Null(const_cast<Pointer *>(this));
}

IR::Val Function::EmitInitialValue() const {
  return IR::Val::Func((IR::Func *)nullptr);
}

IR::Val Array::EmitInitialValue() const {
  auto current_block   = IR::Block::Current;
  IR::Block::Current   = IR::Func::Current->entry();
  auto temp_allocation = IR::Alloca(const_cast<Array *>(this));
  IR::Block::Current   = current_block;

  // TODO must remember to destroy
  const_cast<Array *>(this)->EmitInit(temp_allocation);
  return temp_allocation;
}

IR::Val Struct::EmitInitialValue() const {
  auto current_block   = IR::Block::Current;
  IR::Block::Current   = IR::Func::Current->entry();
  auto temp_allocation = IR::Alloca(const_cast<Struct *>(this));
  IR::Block::Current   = current_block;

  // TODO must remember to destroy
  const_cast<Struct *>(this)->EmitInit(temp_allocation);
  return temp_allocation;
}

IR::Val Tuple::EmitInitialValue() const { NOT_YET; }
IR::Val RangeType::EmitInitialValue() const { NOT_YET; }
IR::Val SliceType::EmitInitialValue() const { NOT_YET; }
IR::Val Scope_Type::EmitInitialValue() const { NOT_YET; }