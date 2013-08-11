#include "pj_ast_terms.h"

#include <stdio.h>
#include <stdlib.h>

using namespace PerlJIT;
using namespace PerlJIT::AST;

/* keep in sync with pj_op_type in .h file */
const char *pj_ast_op_names[] = {
  /* unops */
  "unary -",  /* pj_unop_negate */
  "sin",      /* pj_unop_sin */
  "cos",      /* pj_unop_cos */
  "abs",      /* pj_unop_abs */
  "sqrt",     /* pj_unop_sqrt */
  "log",      /* pj_unop_log */
  "exp",      /* pj_unop_exp */
  "int",      /* pj_unop_int */
  "~",        /* pj_unop_bitwise_not */
  "!",        /* pj_unop_bool_not */

  /* binops */
  "+",        /* pj_binop_add */
  "-",        /* pj_binop_subtract */
  "*",        /* pj_binop_multiply */
  "/",        /* pj_binop_divide */
  "%",        /* pj_binop_modulo */
  "atan2",    /* pj_binop_atan2 */
  "pow",      /* pj_binop_pow */
  "<<",       /* pj_binop_left_shift */
  ">>",       /* pj_binop_right_shift */
  "&",        /* pj_binop_bitwise_and */
  "|",        /* pj_binop_bitwise_or */
  "^",        /* pj_binop_bitwise_xor */
  "==",       /* pj_binop_eq */
  "!=",       /* pj_binop_ne */
  "<",        /* pj_binop_lt */
  "<=",       /* pj_binop_le */
  ">",        /* pj_binop_gt */
  ">=",       /* pj_binop_ge */
  "&&",       /* pj_binop_bool_and */
  "||",       /* pj_binop_bool_or */

  /* listops */
  "?:",       /* pj_listop_ternary */
};

unsigned int pj_ast_op_flags[] = {
  /* unops */
  0,                              /* pj_unop_negate */
  0,                              /* pj_unop_sin */
  0,                              /* pj_unop_cos */
  0,                              /* pj_unop_abs */
  0,                              /* pj_unop_sqrt */
  0,                              /* pj_unop_log */
  0,                              /* pj_unop_exp */
  0,                              /* pj_unop_int */
  0,                              /* pj_unop_bitwise_not */
  0,                              /* pj_unop_bool_not */

  /* binops */
  0,                              /* pj_binop_add */
  0,                              /* pj_binop_subtract */
  0,                              /* pj_binop_multiply */
  0,                              /* pj_binop_divide */
  0,                              /* pj_binop_modulo */
  0,                              /* pj_binop_atan2 */
  0,                              /* pj_binop_pow */
  0,                              /* pj_binop_left_shift */
  0,                              /* pj_binop_right_shift */
  0,                              /* pj_binop_bitwise_and */
  0,                              /* pj_binop_bitwise_or */
  0,                              /* pj_binop_bitwise_xor */
  0,                              /* pj_binop_eq */
  0,                              /* pj_binop_ne */
  0,                              /* pj_binop_lt */
  0,                              /* pj_binop_le */
  0,                              /* pj_binop_gt */
  0,                              /* pj_binop_ge */
  PJ_ASTf_CONDITIONAL,            /* pj_binop_bool_and */
  PJ_ASTf_CONDITIONAL,            /* pj_binop_bool_or */

  /* listops */
  PJ_ASTf_CONDITIONAL,            /* pj_listop_ternary */
};


Constant::Constant(OP *p_op, double c)
  : Term(p_op, pj_ttype_constant), dbl_value(c),
    const_type(pj_double_type)
{}

Constant::Constant(OP *p_op, int c)
  : Term(p_op, pj_ttype_constant), int_value(c),
    const_type(pj_int_type)
{}

Constant::Constant(OP *p_op, unsigned int c)
  : Term(p_op, pj_ttype_constant), uint_value(c),
    const_type(pj_uint_type)
{}


PerlJIT::AST::Term *
pj_make_variable(OP *perl_op, int iv, pj_basic_type t)
{
  PerlJIT::AST::Variable *v = new PerlJIT::AST::Variable();
  v->type = pj_ttype_variable;
  v->perl_op = perl_op;
  v->var_type = t;
  v->ivar = iv;
  return (PerlJIT::AST::Term *)v;
}


PerlJIT::AST::Term *
pj_make_binop(OP *perl_op, pj_optype t, PerlJIT::AST::Term *o1, PerlJIT::AST::Term *o2)
{
  PerlJIT::AST::Op *o = new PerlJIT::AST::Binop();
  o->type = pj_ttype_op;
  o->perl_op = perl_op;
  o->optype = t;
  o->kids.resize(2);
  o->kids[0] = o1;
  o->kids[1] = o2;
  return (PerlJIT::AST::Term *)o;
}


PerlJIT::AST::Term *
pj_make_unop(OP *perl_op, pj_optype t, PerlJIT::AST::Term *o1)
{
  PerlJIT::AST::Op *o = new PerlJIT::AST::Unop();
  o->type = pj_ttype_op;
  o->perl_op = perl_op;
  o->optype = t;
  o->kids.resize(1);
  o->kids[0] = o1;
  return (PerlJIT::AST::Term *)o;
}


PerlJIT::AST::Term *
pj_make_listop(OP *perl_op, pj_optype t, const std::vector<PerlJIT::AST::Term *> &children)
{
  PerlJIT::AST::Op *o = new PerlJIT::AST::Listop();
  o->type = pj_ttype_op;
  o->perl_op = perl_op;
  o->optype = t;
  o->kids = children;
  return (PerlJIT::AST::Term *)o;
}


PerlJIT::AST::Term *
pj_make_optree(OP *perl_op)
{
  PerlJIT::AST::Op *o = new PerlJIT::AST::Op();
  o->type = pj_ttype_optree;
  o->perl_op = perl_op;
  return (PerlJIT::AST::Term *)o;
}


void
pj_free_tree(PerlJIT::AST::Term *t)
{
  if (t == NULL)
    return;

  if (t->type == pj_ttype_op) {
    std::vector<PerlJIT::AST::Term *> &k = ((PerlJIT::AST::Op *)t)->kids;
    const unsigned int n = k.size();
    for (unsigned int i = 0; i < n; ++i)
      pj_free_tree(k[i]);
  }

  delete t;
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
pj_dump_tree_internal(PerlJIT::AST::Term *term, int lvl)
{
  if (term->type == pj_ttype_constant)
  {
    PerlJIT::AST::Constant *c = (PerlJIT::AST::Constant *)term;
    pj_dump_tree_indent(lvl);
    if (c->const_type == pj_double_type)
      printf("C = %f\n", (float)c->dbl_value);
    else if (c->const_type == pj_int_type)
      printf("C = %i\n", (int)c->int_value);
    else if (c->const_type == pj_uint_type)
      printf("C = %lu\n", (unsigned long)c->uint_value);
    else
      abort();
  }
  else if (term->type == pj_ttype_variable)
  {
    pj_dump_tree_indent(lvl);
    printf("V = %i\n", ((PerlJIT::AST::Variable *)term)->ivar);
  }
  else if (term->type == pj_ttype_op)
  {
    PerlJIT::AST::Op *o = (PerlJIT::AST::Op *)term;

    pj_dump_tree_indent(lvl);

    printf("OP '%s' (\n", pj_ast_op_names[o->optype]);

    const unsigned int n = o->kids.size();
    for (unsigned int i = 0; i < n; ++i) {
      pj_dump_tree_internal(o->kids[i], lvl+1);
    }

    pj_dump_tree_indent(lvl);
    printf(")\n");
  }
  else
    abort();
}


void
pj_dump_tree(PerlJIT::AST::Term *term)
{
  int lvl = 0;
  pj_dump_tree_internal(term, lvl);
}


void
Term::dump()
{
  pj_dump_tree(this);
}

