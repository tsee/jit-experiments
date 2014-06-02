#ifndef _PJ_AST_MAP
#define _PJ_AST_MAP

#include "pj_ast_terms.h"

namespace PerlJIT {
  struct CodegenNode {
    enum Type {
      INVALID = -1,
      TERM    = 1,
      TYPE    = 2,
      VALUE   = 3,
    };

    Type value_kind;
    union {
      PerlJIT::AST::Term *term;
      PerlJIT::AST::Type *type;
      int value;
    };

    CodegenNode() :
      value_kind(INVALID)
    { }

    CodegenNode(PerlJIT::AST::Term *_term) :
      value_kind(TERM), term(_term)
    { }

    CodegenNode(PerlJIT::AST::Type *_type) :
      value_kind(TYPE), type(_type)
    { }

    CodegenNode(int _value) :
      value_kind(VALUE), value(_value)
    { }
  };

  struct MappedFunctor {
    int functor_id;
    int arity;
    int extra_arity;

    MappedFunctor(int _functor_id, int _arity, int _extra_arity = 0) :
      functor_id(_functor_id), arity(_arity), extra_arity(_extra_arity)
    { }
  };

  CodegenNode map_codegen_arg(CodegenNode node, int functor_id, int index);
  MappedFunctor map_codegen_functor(CodegenNode node);
}

#endif // _PJ_AST_MAP
