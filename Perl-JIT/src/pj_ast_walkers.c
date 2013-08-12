#include <stdlib.h>
#include <stdio.h>
#include "pj_ast_walkers.h"

static void
pj_tree_extract_vars_internal(PerlJIT::AST::Term *term, PerlJIT::AST::Variable * **vars, unsigned int *nvars)
{
  if (term->type == pj_ttype_variable)
  {
    /* not efficient, but simple */
    if (vars == NULL)
      *vars = (PerlJIT::AST::Variable **)malloc(sizeof(PerlJIT::AST::Variable *));
    else
      *vars = (PerlJIT::AST::Variable **)realloc(*vars, (*nvars+1) * sizeof(PerlJIT::AST::Variable *));
    (*vars)[*nvars] = (PerlJIT::AST::Variable *)term;
    (*nvars)++;
  }
  else if (term->type == pj_ttype_op)
  {
    PerlJIT::AST::Op *o = (PerlJIT::AST::Op *)term;
    const std::vector<PerlJIT::AST::Term *> &kids = o->kids;
    const unsigned int n = kids.size();
    for (unsigned int i = 0; i < n; ++i)
      pj_tree_extract_vars_internal(kids[i], vars, nvars);
  }
}

void
pj_tree_extract_vars(PerlJIT::AST::Term *term, PerlJIT::AST::Variable * **vars, unsigned int *nvars)
{
  *nvars = 0;
  *vars = NULL;
  pj_tree_extract_vars_internal(term, vars, nvars);
}

/* FIXME this isn't really very useful right now and if it becomes that,
 *       it could really do with a rewrite */
pj_basic_type
pj_tree_determine_funtype(PerlJIT::AST::Term *term)
{
  if (term->type == pj_ttype_variable) {
    return ((PerlJIT::AST::Variable *)term)->var_type;
  }
  else if (term->type == pj_ttype_constant) {
    return ((PerlJIT::AST::Constant *)term)->const_type;
  }
  else if (term->type == pj_ttype_op) {
    PerlJIT::AST::Op *o = (PerlJIT::AST::Op *)term;
    pj_basic_type t1;
    t1 = pj_tree_determine_funtype(o->kids[0]);
    if (t1 == pj_double_type)
      return pj_double_type; /* double >> int */

    if (o->op_class() == pj_opc_listop) {
      const std::vector<PerlJIT::AST::Term *> &kids = o->kids;
      const unsigned int n = kids.size();
      for (unsigned int i = 0; i < n; ++i) {
        pj_basic_type t = pj_tree_determine_funtype(kids[i]);
        if (t == pj_double_type)
          return pj_double_type;
        if (t != t1)
          return pj_double_type; /* FIXME correct? */
      }
    }
    else if (o->op_class() == pj_opc_binop) {
      pj_basic_type t2 = pj_tree_determine_funtype(o->kids[1]);
      if (t2 == pj_double_type)
        return pj_double_type;
      else if (t1 == t2)
        return t1;
    }
    else return t1; /* unop */
  }
  return pj_int_type; /* no uint support yet */
}

