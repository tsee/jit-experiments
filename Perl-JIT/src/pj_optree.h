#ifndef PJ_OPTREE_H_
#define PJ_OPTREE_H_

#include "EXTERN.h"
#include "perl.h"

#include "pj_ast_terms.h"

#include <vector>

/* Code relating to traversing and manipulating the OP tree */

/* Starting from root OP, traverse the tree to find candidate OP for JITing
 * and perform actual replacement if at all. */
/* This function will internally call pj_attempt_jit on candidates,
 * which will, in turn, call this function on subtrees that it cannot
 * JIT. */
std::vector<PerlJIT::AST::Term *> pj_find_jit_candidates(pTHX_ OP *o, OP *parentop);

std::vector<PerlJIT::AST::Term *> pj_find_jit_candidates(pTHX_ SV *coderef);

#endif
