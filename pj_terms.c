#include "pj_terms.h"

#include <stdio.h>
#include <stdlib.h>

pj_const_t *
pj_make_const(double c)
{
  pj_const_t *co = (pj_const_t *)malloc(sizeof(pj_const_t));
  co->type = pj_t_constant;
  co->value = c;
  return co;
}


pj_var_t *
pj_make_variable(int iv)
{
  pj_var_t *v = (pj_var_t *)malloc(sizeof(pj_var_t));
  v->type = pj_t_variable;
  v->ivar = iv;
  return v;
}


pj_binop_t *
pj_make_binop(pj_optype t, pj_term_t *o1, pj_term_t *o2)
{
  pj_binop_t *o = (pj_binop_t *)malloc(sizeof(pj_binop_t));
  o->type = pj_t_binop;
  o->optype = t;
  o->op1 = o1;
  o->op2 = o2;
  return o;
}


void
pj_free_tree(pj_term_t *t)
{
  if (t->type == pj_t_binop) {
    pj_free_tree(((pj_binop_t *)t)->op1);
    pj_free_tree(((pj_binop_t *)t)->op2);
  }

  free(t);
}


/* pinnacle of software engineering, but it's just for debugging anyway...  */
static void
pj_dump_tree_indent(int lvl)
{
  int i;
  for (i = 0; i < lvl; ++i)
    printf("  ");
}


static void
pj_dump_tree_internal(pj_term_t *term, int lvl)
{
  if (term->type == pj_t_constant)
  {
    pj_dump_tree_indent(lvl);
    printf("C = %f\n", ((pj_const_t *)term)->value);
  }
  else if (term->type == pj_t_variable)
  {
    pj_dump_tree_indent(lvl);
    printf("V = %i\n", ((pj_var_t *)term)->ivar);
  }
  else if (term->type == pj_t_binop)
  {
    pj_binop_t *b = (pj_binop_t *)term;

    pj_dump_tree_indent(lvl);

    printf("B '");
    if (b->optype == pj_binop_add)
      printf("+");
    else if (b->optype == pj_binop_subtract)
      printf("-");
    else if (b->optype == pj_binop_multiply)
      printf("*");
    else if (b->optype == pj_binop_divide)
      printf("/");
    else
      abort();

    printf("' (\n");
    pj_dump_tree_internal(b->op1, lvl+1);
    pj_dump_tree_internal(b->op2, lvl+1);
    pj_dump_tree_indent(lvl);
    printf(")\n");
  }
  else
    abort();
}


void
pj_dump_tree(pj_term_t *term)
{
  int lvl = 0;
  pj_dump_tree_internal(term, lvl);
}
