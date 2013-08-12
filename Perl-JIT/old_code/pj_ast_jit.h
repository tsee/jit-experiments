#ifndef PJ_JIT_H_
#define PJ_JIT_H_

#include <pj_ast_terms.h>
#include <jit/jit.h>

/* Generates outfun and funtype. funtype indicates the type of all parameters as well
 * as the return value. That's a very serious limitation, but perfectly good enough for
 * now. funtype will be int if all variables and constants are int, otherwise double. */
int pj_tree_jit(jit_context_t context,
                PerlJIT::AST::Term *term,
                jit_function_t *outfun,
                pj_basic_type *funtype);

typedef void (*pj_invoke_func_t)(void);

/* thanks to the saddest code generation on the
 * planet, this can handle up to 20 args right now (see make regen) */
void pj_invoke_func(pj_invoke_func_t fptr,
                    void *args,
                    unsigned int nargs,
                    pj_basic_type funtype,
                    void *retval);

#endif
