#ifndef PJ_EMITVALUE_H_
#define PJ_EMITVALUE_H_

#include "pj_ast_terms.h"

#include <llvm/IR/Value.h>

namespace PerlJIT {
  struct EmitValue {
    llvm::Value *value;
    const PerlJIT::AST::Type *type;

    EmitValue() : value(NULL), type(NULL) { }
    EmitValue(llvm::Value *_value, const PerlJIT::AST::Type *_type) :
      value(_value), type(_type) { }

    bool is_invalid() const { return type == NULL; }
    static EmitValue invalid() { return EmitValue(0, 0); }
  };
}

#endif
