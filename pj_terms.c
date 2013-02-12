#include "pj_terms.h"

#include <stdio.h>
#include <stdlib.h>

pj_term_t *
pj_make_const_dbl(double c)
{
  pj_constant_t *co = (pj_constant_t *)malloc(sizeof(pj_constant_t));
  co->type = pj_ttype_constant;
  co->value_u.dbl_value = c;
  co->const_type = pj_double_type;
  return (pj_term_t *)co;
}

pj_term_t *
pj_make_const_int(int c)
{
  pj_constant_t *co = (pj_constant_t *)malloc(sizeof(pj_constant_t));
  co->type = pj_ttype_constant;
  co->value_u.int_value = c;
  co->const_type = pj_int_type;
  return (pj_term_t *)co;
}

pj_term_t *
pj_make_const_uint(unsigned int c)
{
  pj_constant_t *co = (pj_constant_t *)malloc(sizeof(pj_constant_t));
  co->type = pj_ttype_constant;
  co->value_u.uint_value = c;
  co->const_type = pj_uint_type;
  return (pj_term_t *)co;
}


pj_term_t *
pj_make_variable(int iv, pj_basic_type t)
{
  pj_variable_t *v = (pj_variable_t *)malloc(sizeof(pj_variable_t));
  v->type = pj_ttype_variable;
  v->var_type = t;
  v->ivar = iv;
  return (pj_term_t *)v;
}


pj_term_t *
pj_make_binop(pj_optype t, pj_term_t *o1, pj_term_t *o2)
{
  pj_op_t *o = (pj_op_t *)malloc(sizeof(pj_op_t));
  o->type = pj_ttype_op;
  o->optype = t;
  o->op1 = o1;
  o->op2 = o2;
  return (pj_term_t *)o;
}


pj_term_t *
pj_make_unop(pj_optype t, pj_term_t *o1)
{
  pj_op_t *o = (pj_op_t *)malloc(sizeof(pj_op_t));
  o->type = pj_ttype_op;
  o->optype = t;
  o->op1 = o1;
  o->op2 = NULL;
  return (pj_term_t *)o;
}


void
pj_free_tree(pj_term_t *t)
{
  if (t == NULL)
    return;

  if (t->type == pj_ttype_op) {
    pj_free_tree(((pj_op_t *)t)->op1);
    pj_free_tree(((pj_op_t *)t)->op2);
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
  if (term->type == pj_ttype_constant)
  {
    pj_constant_t *c = (pj_constant_t *)term;
    pj_dump_tree_indent(lvl);
    if (c->const_type == pj_double_type)
      printf("C = %f\n", (float)c->value_u.dbl_value);
    else if (c->const_type == pj_int_type)
      printf("C = %i\n", (int)c->value_u.int_value);
    else if (c->const_type == pj_uint_type)
      printf("C = %lu\n", (unsigned long)c->value_u.uint_value);
    else
      abort();
  }
  else if (term->type == pj_ttype_variable)
  {
    pj_dump_tree_indent(lvl);
    printf("V = %i\n", ((pj_variable_t *)term)->ivar);
  }
  else if (term->type == pj_ttype_op)
  {
    pj_op_t *o = (pj_op_t *)term;

    pj_dump_tree_indent(lvl);

    printf("B '");
    if (o->optype == pj_unop_negate)
      printf("unary -");
    else if (o->optype == pj_unop_sin)
      printf("sin");
    else if (o->optype == pj_unop_cos)
      printf("sin");
    else if (o->optype == pj_unop_abs)
      printf("abs");
    else if (o->optype == pj_unop_sqrt)
      printf("sqrt");
    else if (o->optype == pj_unop_log)
      printf("log");
    else if (o->optype == pj_unop_log)
      printf("exp");

    else if (o->optype == pj_binop_add)
      printf("+");
    else if (o->optype == pj_binop_subtract)
      printf("-");
    else if (o->optype == pj_binop_multiply)
      printf("*");
    else if (o->optype == pj_binop_divide)
      printf("/");
    else if (o->optype == pj_binop_atan2)
      printf("atan2");
    else
      abort();

    printf("' (\n");
    pj_dump_tree_internal(o->op1, lvl+1);
    if (o->op2 != NULL)
      pj_dump_tree_internal(o->op2, lvl+1);

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
