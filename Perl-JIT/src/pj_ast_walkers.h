#ifndef PJ_WALKERS_H_
#define PJ_WALKERS_H_

/* AST walking functions. */

#include <pj_ast_terms.h>

void pj_tree_extract_vars(PerlJIT::AST::Term *term, PerlJIT::AST::Variable * **vars, unsigned int *nvars);

pj_basic_type pj_tree_determine_funtype(PerlJIT::AST::Term *term);

#endif
