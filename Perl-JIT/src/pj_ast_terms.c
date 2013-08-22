#include "pj_ast_terms.h"

#include <stdio.h>
#include <stdlib.h>

using namespace PerlJIT;
using namespace PerlJIT::AST;

/* keep in sync with pj_op_type in .h file */
static const char *pj_ast_op_names[] = {
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
  "defined",  /* pj_unop_defined */

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

static unsigned int pj_ast_op_flags[] = {
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
  PJ_ASTf_KIDS_CONDITIONAL,            /* pj_binop_bool_and */
  PJ_ASTf_KIDS_CONDITIONAL,            /* pj_binop_bool_or */

  /* listops */
  PJ_ASTf_KIDS_CONDITIONAL,            /* pj_listop_ternary */
};


Term::Term(OP *p_op, pj_term_type t, Type *v_type)
  : type(t), perl_op(p_op), _value_type(v_type)
{}

Constant::Constant(OP *p_op, double c)
  : Term(p_op, pj_ttype_constant, new Scalar(pj_double_type)),
    dbl_value(c)
{}

Constant::Constant(OP *p_op, int c)
  : Term(p_op, pj_ttype_constant, new Scalar(pj_int_type)),
    int_value(c)
{}

Constant::Constant(OP *p_op, unsigned int c)
  : Term(p_op, pj_ttype_constant, new Scalar(pj_uint_type)),
    uint_value(c)
{}


Identifier::Identifier(OP *p_op, pj_term_type t, Type *v_type)
: Term(p_op, t, v_type)
{}


VariableDeclaration::VariableDeclaration(OP *p_op, int ivariable)
  : Identifier(p_op, pj_ttype_variabledeclaration, new Scalar(pj_unspecified_type)),
    ivar(ivariable)
{}


Variable::Variable(OP *p_op, VariableDeclaration *decl)
  : Identifier(p_op, pj_ttype_variable), declaration(decl)
{}


Optree::Optree(OP *p_op, OP *p_start_op)
  : Term(p_op, pj_ttype_optree), start_op(p_start_op)
{}


NullOptree::NullOptree(OP *p_op)
  : Term(p_op, pj_ttype_nulloptree)
{}


Op::Op(OP *p_op, pj_op_type t)
  : Term(p_op, pj_ttype_op), optype(t)
{}

Unop::Unop(OP *p_op, pj_op_type t, Term *kid)
  : Op(p_op, t)
{
  kids.resize(1);
  kids[0] = kid;
}


Binop::Binop(OP *p_op, pj_op_type t, Term *kid1, Term *kid2)
  : Op(p_op, t)
{
  kids.resize(2);
  kids[0] = kid1;
  kids[1] = kid2;
}


Listop::Listop(OP *p_op, pj_op_type t, const std::vector<Term *> &children)
  : Op(p_op, t)
{
  kids = children;
}

/* pinnacle of software engineering, but it's just for debugging anyway...  */
static void
S_dump_tree_indent(int lvl)
{
  int i;
  printf("# ");
  for (i = 0; i < lvl; ++i)
    printf("  ");
}


void
Constant::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  if (this->_value_type->tag() == pj_double_type)
    printf("C = %f\n", (float)this->dbl_value);
  else if (this->_value_type->tag() == pj_int_type)
    printf("C = %i\n", (int)this->int_value);
  else if (this->_value_type->tag() == pj_uint_type)
    printf("C = %lu\n", (unsigned long)this->uint_value);
  else
    abort();
}


void
VariableDeclaration::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("VD = %i\n", this->ivar);
}


void
Variable::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("V = %i\n", this->declaration->ivar);
}


void
Optree::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("'UnJITable subtree'\n");
}


void
NullOptree::dump(int indent_lvl)
{
  printf("'UnJITable nulled subtree'\n");
}


static void
S_dump_op(PerlJIT::AST::Op *o, const char *op_str, int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("%s '%s' (\n", op_str, o->name());
  const unsigned int n = o->kids.size();
  for (unsigned int i = 0; i < n; ++i) {
    o->kids[i]->dump(indent_lvl+1);
  }
  S_dump_tree_indent(indent_lvl);
  printf(")\n");
}


void Unop::dump(int indent_lvl)
{ S_dump_op(this, "Unop", indent_lvl); }

void Binop::dump(int indent_lvl)
{ S_dump_op(this, "Binop", indent_lvl); }

void Listop::dump(int indent_lvl)
{ S_dump_op(this, "Listop", indent_lvl); }


Term::~Term()
{
  delete _value_type;
}

Op::~Op()
{
  std::vector<PerlJIT::AST::Term *> &k = kids;
  const unsigned int n = k.size();
  for (unsigned int i = 0; i < n; ++i)
    delete k[i];
}

Type *Term::get_value_type()
{
  return _value_type;
}

void Term::set_value_type(Type *t)
{
  delete _value_type;
  _value_type = t;
}

const char *
Op::name()
{
  return pj_ast_op_names[this->optype];
}

unsigned int
Op::flags()
{
  return pj_ast_op_flags[this->optype];
}


