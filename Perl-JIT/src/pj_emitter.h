#ifndef PJ_EMITTER_H_
#define PJ_EMITTER_H_

#include "EXTERN.h"
#include "perl.h"

#undef Copy

#include "pj_optree.h"
#include "pj_perlapi.h"
#include "pj_types.h"
#include "pj_emitvalue.h"
#include "pj_codegen.h"
// thx_member.h also defines the CXT_ARG_/DECL_CXT_MEMBER macros
#include "thx_member.h"

#include <llvm/IR/Module.h>
#include <llvm/PassManager.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

#include <tr1/memory>

namespace PerlJIT {
  class Cxt;

  llvm::Value * const OPTREE = (llvm::Value *)1;

  class Emitter : public Codegen {
  public:
    Emitter(pTHX_ CXT_ARG_(Cxt) CV *cv);
    Emitter(pTHX_ CXT_ARG_(Cxt) const Emitter &other);
    ~Emitter();

    bool process_jit_candidates(const std::vector<PerlJIT::AST::Term *> &asts);
    std::string error() const;

  private:
    void replace_sequence(OP *first, OP *last, OP *ok, int flags);
    void detach_tree(OP *op, int flags);
    void set_error(const std::string &error);

    // generated
    virtual void reduce_action(State *state, int rule_id);

    bool _jit_emit_return(PerlJIT::AST::Term *ast, pj_op_context context, llvm::Value *value, const PerlJIT::AST::Type *type);
    EmitValue _jit_emit_binop(PerlJIT::AST::Binop *ast, const EmitValue &lv, const EmitValue &rv, const PerlJIT::AST::Type *type);
    EmitValue _jit_emit_num_comparison(PerlJIT::AST::Binop *ast, const EmitValue &lv, const EmitValue &rv, const PerlJIT::AST::Type *arg_type);
    EmitValue _jit_emit_logop(PerlJIT::AST::Binop *ast, State *l, State *r, const PerlJIT::AST::Type *type);
    EmitValue _jit_emit_ternary(PerlJIT::AST::Listop *ast, const EmitValue &condition, State *l, State *r, const PerlJIT::AST::Type *type);
    EmitValue _jit_emit_unop(PerlJIT::AST::Unop *ast, const EmitValue &v, const PerlJIT::AST::Type *type);
    EmitValue _jit_emit_optree(State *state);
    EmitValue _jit_emit_root_optree(State *state);
    bool _jit_emit_optree_args(State *state);

    OP *_jit_find_op_next(OP *op);
    EmitValue _jit_emit_const(PerlJIT::AST::Constant *ast, const PerlJIT::AST::Type *type);
    EmitValue _jit_get_lexical_sv(PerlJIT::AST::Lexical *ast);
    EmitValue _jit_get_lexical_declaration_sv(PerlJIT::AST::VariableDeclaration *ast);
    EmitValue _jit_emit_statement(PerlJIT::AST::Statement *ast, State *expression);
    bool _jit_assign_sv(llvm::Value *sv, llvm::Value *value, const PerlJIT::AST::Type *type);

    llvm::Value *_to_nv_value(llvm::Value *value, const PerlJIT::AST::Type *type);
    llvm::Value *_to_iv_value(llvm::Value *value, const PerlJIT::AST::Type *type);
    llvm::Value *_to_bool_value(llvm::Value *value, const PerlJIT::AST::Type *type);
    llvm::Value *_bool_to_scalar_value(llvm::Value *value);
    llvm::Value *_to_type_value(llvm::Value *value, const PerlJIT::AST::Type *type, const PerlJIT::AST::Type *target);
    EmitValue _jit_emit_perl_int(const EmitValue &v);

    llvm::Type *_map_to_llvm_type(const PerlJIT::AST::Type *type);

    llvm::BasicBlock *_create_basic_block();

    void _push_empty_function();
    void _pop_empty_function();
    void _emit_current_function_and_push_empty(PerlJIT::AST::Term *first, PerlJIT::AST::Term *last, const EmitValue &value);
    OP *_replace_with_clean_emitter_state(PerlJIT::AST::Term *root, const EmitValue &value);
    OP *_generate_current_function(PerlJIT::AST::Term *root, const EmitValue &value);

    struct FunctionState {
        llvm::Function *function;
        llvm::BasicBlock *insertion_point;
        std::vector<OP *> subtrees;

        bool empty();
    };

    CV *cv;
    llvm::Module *module;
    llvm::FunctionPassManager *fpm;
    std::tr1::shared_ptr<llvm::ExecutionEngine> execution_engine;
    PerlJIT::PerlAPI &pa;
    std::vector<FunctionState> emitter_states;
    std::string error_message;
    DECL_CXT_MEMBER(Cxt)
    DECL_THX_MEMBER
  };
}

#endif
