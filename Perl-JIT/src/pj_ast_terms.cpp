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

Empty::Empty()
  : Term(NULL, pj_ttype_empty)
{}

List::List()
  : Term(NULL, pj_ttype_list)
{}

List::List(const std::vector<Term *> &kid_terms)
  : Term(NULL, pj_ttype_list), kids(kid_terms)
{}

Constant::Constant(OP *p_op, Type *v_type)
  : Term(p_op, pj_ttype_constant, v_type)
{}

NumericConstant::NumericConstant(OP *p_op, NV c)
  : Constant(p_op, new Scalar(pj_double_type)),
    dbl_value(c)
{}

NumericConstant::NumericConstant(OP *p_op, IV c)
  : Constant(p_op, new Scalar(pj_int_type)),
    int_value(c)
{}

NumericConstant::NumericConstant(OP *p_op, UV c)
  : Constant(p_op, new Scalar(pj_uint_type)),
    uint_value(c)
{}

StringConstant::StringConstant(OP *p_op, const std::string& s, bool isUTF8)
  : Constant(p_op, new Scalar(pj_string_type)),
    string_value(s), is_utf8(isUTF8)
{}

StringConstant::StringConstant(pTHX_ OP *p_op, SV *string_literal_sv)
  : Constant(p_op, new Scalar(pj_string_type))
{
  STRLEN l;
  char *s;
  s = SvPV(string_literal_sv, l);
  string_value = std::string(s, (size_t)l);
  is_utf8 = (bool)SvUTF8(string_literal_sv);
}

UndefConstant::UndefConstant()
  : Constant(NULL, new Scalar(pj_unspecified_type))
{
}


Identifier::Identifier(OP *p_op, pj_term_type t, pj_variable_sigil s, Type *v_type)
  : Term(p_op, t, v_type), sigil(s)
{}


VariableDeclaration::VariableDeclaration(OP *p_op, int ivariable, pj_variable_sigil sigil, Type *v_type)
  : Identifier(p_op, pj_ttype_variabledeclaration, sigil, v_type),
    ivar(ivariable)
{}


Lexical::Lexical(OP *p_op, VariableDeclaration *decl)
  : Identifier(p_op, pj_ttype_lexical, decl->sigil), declaration(decl)
{}


Global::Global(OP *p_op, pj_variable_sigil s)
  : Identifier(p_op, pj_ttype_global, s)
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
  : Op(p_op, t), is_assign_form(false)
{
  // assert that kids are optional if at least one NULL passed
  assert( (kid1 != NULL && kid2 != NULL)
          || (this->flags() & PJ_ASTf_KIDS_OPTIONAL) );
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


Block::Block(OP *p_op, Term *statements)
  : Op(p_op, pj_op_scope)
{
  kids.resize(1);
  kids[0] = statements;
}


BareBlock::BareBlock(OP *p_op, Term *_body, Term *_continuation)
  : Term(p_op, pj_ttype_bareblock), body(_body), continuation(_continuation)
{}


While::While(OP *p_op, Term *_condition, bool _negated,
             Term *_body, Term *_continuation)
  : Term(p_op, pj_ttype_while), condition(_condition), negated(_negated),
    body(_body), continuation(_continuation)
{}


For::For(OP *p_op, Term *_init, Term *_condition, Term *_step, Term *_body)
  : Term(p_op, pj_ttype_for), init(_init), condition(_condition),
    step(_step), body(_body)
{}


Statement::Statement(OP *p_nextstate, Term *term)
  : Term(p_nextstate, pj_ttype_statement)
{
  kids.resize(1);
  kids[0] = term;
}

StatementSequence::StatementSequence()
  : Term(NULL, pj_ttype_statementsequence)
{}

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
NumericConstant::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  if (this->_value_type->tag() == pj_double_type)
    printf("C = (NV)%f\n", (float)this->dbl_value);
  else if (this->_value_type->tag() == pj_int_type)
    printf("C = (IV)%i\n", (int)this->int_value);
  else if (this->_value_type->tag() == pj_uint_type)
    printf("C = (UV)%lu\n", (unsigned long)this->uint_value);
  else
    abort();
}


void
StringConstant::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("C = (string, %s)\"%*s\"\n",
         (is_utf8 ? "UTF8" : "binary"),
         (int)string_value.length(),
         string_value.c_str());
}


void
UndefConstant::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("<undef>\n");
}


static inline char
S_sigil_character(pj_variable_sigil sigil)
{
  switch (sigil) {
  case pj_sigil_scalar:
    return '$';
  case pj_sigil_array:
    return '@';
  case pj_sigil_hash:
    return '%';
  case pj_sigil_glob:
    return '*';
  }
  abort();
  return '\0';
}

void
VariableDeclaration::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("VD (%c) = %i", S_sigil_character(sigil), this->ivar);
  if (Type *value_type = get_value_type())
    printf(" : %s", value_type->to_string().c_str());
  printf("\n");
}


void
Lexical::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("V (%c) = %i", S_sigil_character(declaration->sigil), declaration->ivar);
  if (Type *value_type = get_value_type())
    printf(" : %s", value_type->to_string().c_str());
  printf("\n");
}


void
Global::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("G = (%c)\n", S_sigil_character(sigil));
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

void
Statement::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("Statement %s:%d\n", CopFILE(cCOPx(perl_op)), CopLINE(cCOPx(perl_op)));
  kids[0]->dump(indent_lvl+1);
}

void
StatementSequence::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("StatementSequence\n");
  for (size_t i = 0; i < kids.size(); ++i)
    kids[i]->dump(indent_lvl+1);
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

void Block::dump(int indent_lvl)
{ S_dump_op(this, "Block", false, indent_lvl); }

void BareBlock::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("BareBlock (\n");
  body->dump(indent_lvl + 1);
  if (continuation->type != pj_ttype_empty) {
    S_dump_tree_indent(indent_lvl);
    printf(") Continue (\n");
    continuation->dump(indent_lvl + 1);
  }
  S_dump_tree_indent(indent_lvl);
  printf(")\n");
}

void While::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("%s (\n", negated ? "Until": "While");
  condition->dump(indent_lvl + 2);
  body->dump(indent_lvl + 1);
  if (continuation->type != pj_ttype_empty) {
    S_dump_tree_indent(indent_lvl);
    printf(") Continue (\n");
    continuation->dump(indent_lvl + 1);
  }
  S_dump_tree_indent(indent_lvl);
  printf(")\n");
}

void For::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("For (\n");
  init->dump(indent_lvl + 2);
  condition->dump(indent_lvl + 2);
  step->dump(indent_lvl + 2);
  body->dump(indent_lvl + 1);
  S_dump_tree_indent(indent_lvl);
  printf(")\n");
}

void Empty::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("Empty\n");
}

void List::dump(int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf("List(\n");
  const unsigned int n = this->kids.size();
  for (unsigned int i = 0; i < n; ++i) {
    if (this->kids[i] == NULL) {
      S_dump_tree_indent(indent_lvl+1);
      printf("NULL\n");
    } else {
      this->kids[i]->dump(indent_lvl+1);
    }
  }
  S_dump_tree_indent(indent_lvl);
  printf(")\n");
}

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

List::~List()
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

pj_op_context Term::context()
{
  return pj_op_context(OP_GIMME(perl_op, pj_context_caller));
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
  return (
    is_assign_form
    || ( (flags() & PJ_ASTf_HAS_ASSIGNMENT_FORM) &&
         (perl_op->op_flags & OPf_STACKED) )
  );
}

void
Binop::set_assignment_form(bool is_assignment)
{
  is_assign_form = is_assignment;
}

bool
Binop::is_synthesized_assignment() const
{
  return optype == pj_binop_sassign && kids[1]->perl_op == perl_op;
}

int Lexical::get_pad_index() const
{
  return perl_op->op_targ;
}

#ifdef USE_ITHREADS
int Global::get_pad_index() const
{
  if (perl_op->op_type == OP_RV2AV || perl_op->op_type == OP_RV2HV)
    return cPADOPx(cUNOPx(perl_op)->op_first)->op_padix;
  else
    return cPADOPx(perl_op)->op_padix;
}
#else
GV *Global::get_gv() const
{
  if (perl_op->op_type == OP_RV2AV || perl_op->op_type == OP_RV2HV)
    return cGVOPx_gv(cUNOPx(perl_op)->op_first);
  else
    return cGVOPx_gv(perl_op);
}
#endif

std::vector<PerlJIT::AST::Term *> BareBlock::get_kids()
{
  std::vector<PerlJIT::AST::Term *> kids;

  kids.push_back(body);
  if (continuation->type != pj_ttype_empty)
    kids.push_back(continuation);

  return kids;
}

std::vector<PerlJIT::AST::Term *> While::get_kids()
{
  std::vector<PerlJIT::AST::Term *> kids;

  kids.push_back(condition);
  kids.push_back(body);
  if (continuation->type != pj_ttype_empty)
    kids.push_back(continuation);

  return kids;
}

std::vector<PerlJIT::AST::Term *> For::get_kids()
{
  std::vector<PerlJIT::AST::Term *> kids;

  kids.push_back(init);
  kids.push_back(condition);
  kids.push_back(step);
  kids.push_back(body);

  return kids;
}
