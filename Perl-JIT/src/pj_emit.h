#ifndef PJ_EMIT_H_
#define PJ_EMIT_H_

#include "EXTERN.h"
#include "perl.h"

#undef Copy

#include "pj_optree.h"
#include "pj_perlapi.h"
#include "pj_types.h"
#include "thx_member.h"

#include <llvm/IR/Module.h>
#include <llvm/PassManager.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace PerlJIT {
  class Emit {
  public:
    Emit();
    ~Emit();

    SV *jit_sub(SV *coderef);

  private:
    llvm::Module *module;
    llvm::FunctionPassManager *fpm;
    llvm::ExecutionEngine *execution_engine;
  };

  struct EmitValue {
    llvm::Value *value;
    const PerlJIT::AST::Type *type;

    EmitValue(llvm::Value *_value, const PerlJIT::AST::Type *_type) :
      value(_value), type(_type) { }

    bool is_invalid() const { return type == NULL; }
    static EmitValue invalid() { return EmitValue(0, 0); }
  };

  class Emitter {
  public:
    Emitter(pTHX_ CV *cv, AV *ops, llvm::Module *module, llvm::FunctionPassManager *fpm, llvm::ExecutionEngine *execution_engine, PerlJIT::PerlAPI *pa);
    Emitter(pTHX_ const Emitter &other);
    ~Emitter();

    bool process_jit_candidates(const std::vector<PerlJIT::AST::Term *> &asts);
    std::string error() const;

  private:
    void replace_sequence(OP *first, OP *last, OP *ok, bool keep);
    void detach_tree(OP *op, bool keep);
    void set_error(const std::string &error);

    bool jit_statement_sequence(const std::vector<PerlJIT::AST::Term *> &asts);
    bool jit_tree(PerlJIT::AST::Term *ast);
    OP *_jit_trees(const std::vector<PerlJIT::AST::Term *> &asts);
    bool _jit_emit_root(PerlJIT::AST::Term *ast);
    bool _jit_emit_return(PerlJIT::AST::Term *ast, pj_op_context context, llvm::Value *value, const PerlJIT::AST::Type *type);
    bool is_jittable(PerlJIT::AST::Term *ast);
    bool needs_excessive_magic(PerlJIT::AST::Op *ast);
    EmitValue _jit_emit(PerlJIT::AST::Term *ast, const PerlJIT::AST::Type *type);
    EmitValue _jit_emit_op(PerlJIT::AST::Op *ast, const PerlJIT::AST::Type *type);
    EmitValue _jit_emit_binop(PerlJIT::AST::Binop *ast, const PerlJIT::AST::Type *type);
    EmitValue _jit_emit_optree_jit_kids(PerlJIT::AST::Term *ast, const PerlJIT::AST::Type *Type);
    EmitValue _jit_emit_optree(PerlJIT::AST::Term *ast);

    void _jit_clear_op_next(OP *op);
    EmitValue _jit_emit_const(PerlJIT::AST::Constant *ast, const PerlJIT::AST::Type *type);
    EmitValue _jit_get_lexical_sv(PerlJIT::AST::Lexical *ast);
    EmitValue _jit_get_lexical_declaration_sv(PerlJIT::AST::VariableDeclaration *ast);
    bool _jit_assign_sv(llvm::Value *sv, llvm::Value *value, const PerlJIT::AST::Type *type);

    llvm::Value *_to_nv_value(llvm::Value *value, const PerlJIT::AST::Type *type);

    CV *cv;
    AV *ops;
    std::vector<OP *> subtrees;
    llvm::Module *module;
    llvm::FunctionPassManager *fpm;
    llvm::ExecutionEngine *execution_engine;
    PerlJIT::PerlAPI &pa;
    std::string error_message;
    DECL_THX_MEMBER
  };
}

#endif
