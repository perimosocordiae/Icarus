#ifndef ICARUS_TYPE_TYPE_H
#define ICARUS_TYPE_TYPE_H

struct Type;
struct Struct;
struct Array;
struct Pointer;
struct Tuple;
struct Function;
struct RangeType;
struct SliceType;
struct Scope_Type;
struct ParamStruct;

extern Type *Err, *Unknown, *Bool, *Char, *Int, *Real, *Code_, *Type_, *Uint,
    *Void, *NullPtr, *String;
struct Scope;

#include "../ast/ast.h"
#include "../base/util.h"
#include "../base/debug.h"
#include "../base/types.h"
#include "../ir/ir.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <sstream>

namespace AST {
struct Declaration;
struct Expression;
struct Identifier;
} // namespace AST

#include "constants_and_enums.h"

extern std::vector<IR::Func *> implicit_functions;
// TODO this is not the right API for mangling.
extern std::string Mangle(const Type *t, bool prefix = true);
extern std::string Mangle(const Function *f, AST::Expression *expr,
                          Scope *starting_scope = nullptr);

extern Pointer *Ptr(Type *t);
extern Array *Arr(Type *t);
extern Array *Arr(Type *t, size_t len);
extern Tuple *Tup(const std::vector<Type *> &t);
extern Function *Func(Type *in, Type *out);
extern Function *Func(std::vector<Type *> in, Type *out);
extern Function *Func(Type *in, std::vector<Type *> out);
extern Function *Func(std::vector<Type *> in, std::vector<Type *> out);
extern RangeType *Range(Type *t);
extern SliceType *Slice(Array *a);
extern Scope_Type *ScopeType(Type *t);

#define ENDING = 0

#define BASIC_METHODS                                                          \
  virtual std::string to_string() const ENDING;                                \
  virtual void EmitInit(IR::Val id_val) ENDING;                                \
  virtual void EmitDestroy(IR::Val id_val) ENDING;                             \
  virtual IR::Val EmitInitialValue() const ENDING;                             \
  virtual void EmitRepr(IR::Val id_val) ENDING

#define TYPE_FNS(name, checkname)                                              \
  name() = delete;                                                             \
  virtual ~name() {}                                                           \
  BASIC_METHODS

struct Type : public base::Cast<Type> {
public:
  Type() {}
  virtual ~Type() {}
  BASIC_METHODS;

  // Assigns val to var. We need this to dispatch based on both the lhs and rhs
  // types. Assume that the types match appropriately. Depending on the types,
  // this will either simply be a store operation or a call to the assignment
  // function.
  static void CallAssignment(Scope *scope, Type *lhs_type, Type *rhs_type,
                             IR::Val from_val, IR::Val to_var);

  // Note: this one is special. It functions identically to the rest, but it's
  // special in that it will return nullptr if you haven't imported the string
  // library. This should never come up, because it's only used to add type to a
  // string literal, and using a string literal should import strings.
  static Type *get_string();

#define PRIMITIVE_MACRO(GlobalName, EnumName, name)                            \
  virtual bool is_##name() const { return false; }
#include "../config/primitive.conf"
#undef PRIMITIVE_MACRO


  virtual bool is_big() const;
  virtual bool stores_data() const;
};

#undef ENDING
#define ENDING

enum class PrimType : char {
#define PRIMITIVE_MACRO(GlobalName, EnumName, name) EnumName,
#include "../config/primitive.conf"
#undef PRIMITIVE_MACRO
};

struct Primitive : public Type {
public:
  TYPE_FNS(Primitive, primitive);
  Primitive(PrimType pt);

#define PRIMITIVE_MACRO(GlobalName, EnumName, name)                            \
  virtual bool is_##name() const;
#include "../config/primitive.conf"
#undef PRIMITIVE_MACRO

private:
  friend class Architecture;
  PrimType type_;

  IR::Func *repr_func = nullptr;
};

struct Array : public Type {
  TYPE_FNS(Array, array);
  Array(Type *t, size_t l);

  IR::Func *init_func = nullptr, *repr_func = nullptr, *destroy_func = nullptr;

  Type *data_type;
  size_t len;
  bool fixed_length;

  // Not the length of the array, but the dimension. That is, it's how many
  // times you can access an element.
  size_t dimension;

  Array(Type *t);
};

struct Tuple : public Type {
  TYPE_FNS(Tuple, tuple);

  Tuple(const std::vector<Type *> &types);

  std::vector<Type *> entries;
};

struct Pointer : public Type {
  TYPE_FNS(Pointer, pointer);

  Pointer(Type *t);
  Type *pointee;
};

struct Function : public Type {
  TYPE_FNS(Function, function);

  Function(Type *in, Type *out);
  Type *input, *output;
};

struct Enum : public Type {
  TYPE_FNS(Enum, enum);
  Enum(const std::string &name, const std::vector<std::string> &members);

  size_t IndexOrFail(const std::string &str) const;
  IR::Val EmitLiteral(const std::string &member_name) const;

  std::string bound_name;
  std::vector<std::string> members;
  std::unordered_map<std::string, size_t> int_values;
};

struct Struct : public Type {
  TYPE_FNS(Struct, struct);

  Struct(const std::string &name);
  static Struct Anon(const std::set<AST::Declaration *> &declarations);

  void EmitDefaultAssign(IR::Val to_var, IR::Val from_val);

  // Return the type of a field, or a nullptr if it doesn't exist
  Type *field(const std::string &name) const;

  size_t field_num(const std::string &name) const;

  void CompleteDefinition();

  Scope *type_scope = nullptr;
  std::vector<AST::Declaration *> decls;

  std::string bound_name;

  void insert_field(const std::string &name, Type *ty,
                    AST::Expression *init_val);

  // Field database info
  std::unordered_map<std::string, size_t> field_name_to_num;
  std::vector<std::string> field_num_to_name;
  std::vector<Type *> field_type;

  std::vector<AST::Expression *> init_values;
  ParamStruct *creator = nullptr;

private:
  IR::Func *init_func = nullptr, *assign_func = nullptr,
           *destroy_func = nullptr, *repr_func = nullptr;
  bool completed_ = false;
};

struct ParamStruct : public Type {
  TYPE_FNS(ParamStruct, parametric_struct);

  ParamStruct(const std::string &name,
                      std::vector<AST::Declaration *> params,
                      std::vector<AST::Declaration *> decls);

  IR::Func *IRFunc();

  std::string bound_name;
  Scope *type_scope = nullptr;
  std::vector<AST::Declaration *> params, decls;
  std::map<std::vector<IR::Val>, Struct *> cache;
  std::unordered_map<Struct *, std::vector<IR::Val>> reverse_cache;

private:
  IR::Func *ir_func = nullptr;
};

struct RangeType : public Type {
  TYPE_FNS(RangeType, range);

  RangeType(Type *t) : end_type(t) {}

  Type *end_type;
};

struct SliceType : public Type {
  TYPE_FNS(SliceType, slice);

  SliceType(Array *a) : array_type(a) {}

  Array *array_type;
};

struct Scope_Type : public Type {
  TYPE_FNS(Scope_Type, scope_type);

  Scope_Type(Type *t) : type_(t) {}
  Type *type_;

  std::string bound_name;
};

std::ostream &operator<<(std::ostream &os, const Type &t);

#undef TYPE_FNS
#undef BASIC_METHODS
#undef ENDING

#endif // ICARUS_TYPE_TYPE_H
