#ifndef PJ_KEYWORD_PLUGIN_H_
#define PJ_KEYWORD_PLUGIN_H_

#include "types.h"

#include <tr1/unordered_map>
#include <EXTERN.h>
#include <perl.h>

namespace PerlJIT {
  // A PADSV OP that introduces a variable together with its
  // declared type.
  class TypedPadSvOp {
  public:
    TypedPadSvOp()
      : fType(NULL), fDeclaration(NULL), fOwnsType(false) {}
    TypedPadSvOp(AST::Type *t, OP *decl)
      : fType(t), fDeclaration(decl), fOwnsType(false) {}

    ~TypedPadSvOp()
      { if (fOwnsType) delete fType; }

    AST::Type *get_type() const    { return fType; }
    OP *get_declaration() const    { return fDeclaration; }
    void set_declaration(OP *decl) { fDeclaration = decl; }
    void set_type(AST::Type *t)    { fType = t; fOwnsType = false; }
    void set_type_ownership(bool own) { fOwnsType = own; }

  private:
    AST::Type *fType;
    OP *fDeclaration;
    bool fOwnsType;
  };
}

// Main keyword plugin hook for JIT type annotations. Will put MAGIC on compiling CV.
int pj_jit_type_keyword_plugin(pTHX_ char *keyword_ptr, STRLEN keyword_len, OP **op_ptr);

// Fetch set of typed variable declarations from CV
std::tr1::unordered_map<PADOFFSET, PerlJIT::TypedPadSvOp> *pj_get_typed_variable_declarations(pTHX_ CV *cv);

#define PJ_KEYWORD_PLUGIN_HINT "PJIT:kw"

#endif
