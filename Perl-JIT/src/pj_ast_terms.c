#include "pj_ast_terms.h"

#include "EXTERN.h"
#include "perl.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

using namespace PerlJIT;
using namespace PerlJIT::AST;

// That file has the static data about AST ops such as their
// human-readable names and their flags.
#include "pj_ast_ops_data-gen.inc"

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


VariableDeclaration::VariableDeclaration(OP *p_op, int ivariable, Type *v_type)
  : Identifier(p_op, pj_ttype_variabledeclaration, v_type),
    ivar(ivariable)
{}


Variable::Variable(OP *p_op, VariableDeclaration *decl)
  : Identifier(p_op, pj_ttype_variable), declaration(decl)
{}


Optree::Optree(OP *p_op)
  : Term(p_op, pj_ttype_optree)
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
  if (kid == NULL) {
    // assert that kids are optional, since NULL passed
    assert(this->flags() & PJ_ASTf_KIDS_OPTIONAL);
  }
  kids.resize(1);
  kids[0] = kid;
}


Binop::Binop(OP *p_op, pj_op_type t, Term *kid1, Term *kid2)
  : Op(p_op, t)
{
  if (kid1 == NULL || kid2 == NULL) {
    // assert that kids are optional, since at least one NULL passed
    assert(this->flags() & PJ_ASTf_KIDS_OPTIONAL);
  }
  kids.resize(2);
  kids[0] = kid1;
  kids[1] = kid2;
}


Listop::Listop(OP *p_op, pj_op_type t, const std::vector<Term *> &children)
  : Op(p_op, t)
{
  // TODO assert() that children aren't NULL _OR_
  //      the PJ_ASTf_KIDS_OPTIONAL flag is set
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
  if (this->declaration)
    printf("V = %i\n", this->declaration->ivar);
  else
    printf("V = ?????? FIXME THIS IS A BUG. Cannot handle pkg vars yet...\n");
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
S_dump_op(PerlJIT::AST::Op *o, const char *op_str, bool is_assignment_form, int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("%s '%s%s' (\n", op_str, o->name(), is_assignment_form ? "=" : "");
  const unsigned int n = o->kids.size();
  for (unsigned int i = 0; i < n; ++i) {
    if (o->kids[i] == NULL) {
      S_dump_tree_indent(indent_lvl+1);
      printf("NULL\n");
    } else {
      o->kids[i]->dump(indent_lvl+1);
    }
  }
  S_dump_tree_indent(indent_lvl);
  printf(")\n");
}


void Unop::dump(int indent_lvl)
{ S_dump_op(this, "Unop", false, indent_lvl); }

void Binop::dump(int indent_lvl)
{ S_dump_op(this, "Binop", is_assignment_form(), indent_lvl); }

void Listop::dump(int indent_lvl)
{ S_dump_op(this, "Listop", false, indent_lvl); }


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

OP *Term::start_op()
{
  OP *o = perl_op;
  while (1) {
    if (o->op_flags & OPf_KIDS)
      o = cUNOPo->op_first;
    else
      return o;
  }
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

bool
Binop::is_assignment_form()
{
  return (flags() & PJ_ASTf_HAS_ASSIGNMENT_FORM) &&
         (perl_op->op_flags & OPf_STACKED);
}
