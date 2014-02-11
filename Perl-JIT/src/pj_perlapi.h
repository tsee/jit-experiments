#ifndef PJ_PERLAPI_H_
#define PJ_PERLAPI_H_

#include <EXTERN.h>
#include <perl.h>

#undef Copy
#undef save_clearsv
#undef sv_nv

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>

namespace PerlJIT {
  class PerlAPI {
  public:
PerlAPI(llvm::Module *module, llvm::IRBuilder<> *builder, llvm::ExecutionEngine *ee);

    llvm::FunctionType *ppaddr_type() const { return pp_type; }
    void set_thx(llvm::Value *thx);

    void push_sv(llvm::Value *value);
    void call_runloop(OP *op);
    void save_clearsv(llvm::Value *svp);

    void sv_set_nv(llvm::Value *sv, llvm::Value *value);
    void sv_set_iv(llvm::Value *sv, llvm::Value *value);
    void sv_set_sv_nosteal(llvm::Value *sv, llvm::Value *value);

    llvm::Value *sv_nv(llvm::Value *sv);

    llvm::Value *get_targ();
    llvm::Value *get_op_next();
    llvm::Value *new_mortal_sv();
    llvm::Value *pop_sv();
    llvm::Value *get_pad_sv(UV padix);
    llvm::Value *get_pad_sv_address(UV padix);

    llvm::Constant *IV_constant(IV value);
    llvm::Constant *UV_constant(UV value);
    llvm::Constant *NV_constant(NV value);
  private:
    llvm::Value *interp_value(unsigned int offset, llvm::Type *type, const llvm::Twine &name);
    llvm::FunctionType *function_type(llvm::Type *ret, ...);

    llvm::Module *module;
    llvm::IRBuilder<> *builder;
    llvm::Value *thx;
    llvm::Type *iv_type, *uv_type, *nv_type, *ptr_type, *byte_type;
    llvm::Type *ptr_ptr_type, *ptr_iv_type, *ptr_uv_type, *ptr_nv_type;
    llvm::FunctionType *pp_type;

    // TODO autogenerate
    llvm::Function *pa_sv_setnv, *pa_sv_setiv, *pa_sv_setsv_nosteal,
        *pa_sv_newmortal, *pa_push_sv, *pa_pop_sv, *pa_call_runloop,
        *pa_save_clearsv, *pa_sv_nv;
  };
}

#endif // PJ_PERLAPI_H_
