#include "pj_snippets.h"

using namespace PerlJIT;
using namespace llvm;

Snippets::Snippets(Module *_module, IRBuilder<> *_builder) :
  module(_module), builder(_builder), function(NULL)
{
#ifdef USE_ITHREADS
  arg_thx = NULL;
#endif
  arg_sp = NULL;
}

void
Snippets::set_current_function(Function *_function)
{
  function = _function;
#ifdef USE_ITHREADS
  arg_thx = function->arg_begin();
#endif
  arg_sp = NULL;
}

Value *
Snippets::alloc_variable(llvm::Type *type, const Twine &name)
{
  return new AllocaInst(type, 0, name, &function->front().front());
}
