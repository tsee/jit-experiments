#ifndef PJ_SNIPPETS_H_
#define PJ_SNIPPETS_H_

#include <EXTERN.h>
#include <perl.h>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>

namespace PerlJIT {
  class Snippets {
  public:
    Snippets(llvm::Module *module, llvm::IRBuilder<> *builder);

    void set_current_function(llvm::Function *function);
    llvm::Value *alloc_variable(llvm::Type *type, const llvm::Twine &name);

  protected:
    llvm::Module *module;
    llvm::IRBuilder<> *builder;
    llvm::Function *function;
#ifdef USE_ITHREADS
    llvm::Value *arg_thx;
#endif
    llvm::Value *arg_sp;
  };
}

#endif // PJ_SNIPPETS_H_
