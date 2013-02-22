#include <stdlib.h>
#include <stdio.h>
#include "pj_ast_walkers.h"

static void
pj_tree_extract_vars_internal(pj_term_t *term, pj_variable_t * **vars, unsigned int *nvars)
{
  if (term->type == pj_ttype_variable)
  {
    /* not efficient, but simple */
    if (vars == NULL)
      *vars = (pj_variable_t **)malloc(sizeof(pj_variable_t *));
    else
      *vars = (pj_variable_t **)realloc(*vars, (*nvars+1) * sizeof(pj_variable_t *));
    (*vars)[*nvars] = (pj_variable_t *)term;
    (*nvars)++;
  }
  else if (term->type == pj_ttype_op)
  {
    pj_op_t *o = (pj_op_t *)term;
    pj_tree_extract_vars_internal(o->op1, vars, nvars);
    if (o->op2 != NULL)
      pj_tree_extract_vars_internal(o->op2, vars, nvars);
  }
}

void
pj_tree_extract_vars(pj_term_t *term, pj_variable_t * **vars, unsigned int *nvars)
{
  *nvars = 0;
  *vars = NULL;
  pj_tree_extract_vars_internal(term, vars, nvars);
}

pj_basic_type
pj_tree_determine_funtype(pj_term_t *term)
{
  if (term->type == pj_ttype_variable) {
    return ((pj_variable_t *)term)->var_type;
  }
  else if (term->type == pj_ttype_constant) {
    return ((pj_constant_t *)term)->const_type;
  }
  else if (term->type == pj_ttype_op) {
    pj_op_t *o = (pj_op_t *)term;
    pj_basic_type t1, t2;
    t1 = pj_tree_determine_funtype(o->op1);
    if (t1 == pj_double_type)
      return pj_double_type; /* double >> int */
    if (o->op2 != NULL) {
      t2 = pj_tree_determine_funtype(o->op2);
      if (t2 == pj_double_type)
        return pj_double_type;
      if (t1==t2) return t1;
    }
    else return t1;
  }
  return pj_int_type; /* no uint support yet */
}

