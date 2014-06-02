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

const std::vector<Term *> Term::empty;

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
  : Term(NULL, pj_ttype_list)
{
  // Flatten nested AST::List's. Need only one level of
  // flattening since they all flattened on construction (hopefully).
  const unsigned int n = kid_terms.size();
  for (unsigned int i = 0; i < n; ++i) {
    if (kid_terms[i]->get_type() == pj_ttype_list) {
      List *l = (List *)kid_terms[i];
      // transfer ownership if sub-elements
      std::vector<Term *> &nested_kids = l->kids;
      for (unsigned int j = 0; j < nested_kids.size(); ++j) {
        kids.push_back(nested_kids[j]);
      }
      nested_kids.clear();
      delete l;
    }
    else
      kids.push_back(kid_terms[i]);
  }
}

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

ArrayConstant::ArrayConstant(OP *p_op, AV *array)
  : Constant(p_op, new Array(new Scalar(pj_unspecified_type))),
    const_array(array)
{}


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
  : Term(p_op, pj_ttype_op), op_type(t), _is_integer_variant(false)
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


Baseop::Baseop(OP *p_op, pj_op_type t)
  : Op(p_op, t)
{
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

Term *BareBlock::get_body() const
{ return body; }

Term *BareBlock::get_continuation() const
{ return continuation; }


While::While(OP *p_op, Term *_condition, bool _negated, bool _evaluate_after,
             Term *_body, Term *_continuation)
  : Term(p_op, pj_ttype_while), condition(_condition),
    negated(_negated), evaluate_after(_evaluate_after),
    body(_body), continuation(_continuation)
{}


For::For(OP *p_op, Term *_init, Term *_condition, Term *_step, Term *_body)
  : Term(p_op, pj_ttype_for), init(_init), condition(_condition),
    step(_step), body(_body)
{}


Foreach::Foreach(OP *p_op, Term *_iterator, Term *_expression, Term *_body, Term *_continuation)
  : Term(p_op, pj_ttype_foreach), iterator(_iterator), expression(_expression),
    body(_body), continuation(_continuation)
{}

ListTransformation::ListTransformation(OP *p_op, pj_term_type type, Term *_body, List *_parameters)
  : Term(p_op, type), body(_body), parameters(_parameters)
{}

Map::Map(OP *p_op, Term *_body, List *_parameters)
  : ListTransformation(p_op, pj_ttype_map, _body, _parameters)
{}

Grep::Grep(OP *p_op, Term *_body, List *_parameters)
  : ListTransformation(p_op, pj_ttype_grep, _body, _parameters)
{}


SubCall::SubCall(OP *entersub_op,
                 PerlJIT::AST::Term *cv_src,
                 const std::vector<PerlJIT::AST::Term *> &args)
  : Term(entersub_op, pj_ttype_function_call), _cv_source(cv_src), _arguments(args)
{}

std::vector<PerlJIT::AST::Term *>
SubCall::get_arguments() const
{
  return _arguments;
}

PerlJIT::AST::Term *
SubCall::get_cv_source() const
{
  return _cv_source;
}


MethodCall::MethodCall(OP *entersub_op,
                       PerlJIT::AST::Term *cv_src,
                       PerlJIT::AST::Term *invocant,
                       const std::vector<PerlJIT::AST::Term *> &args)
  : SubCall(entersub_op, cv_src, args), _invocant(invocant)
{}

PerlJIT::AST::Term *
MethodCall::get_invocant() const
{
  return _invocant;
}

LoopControlStatement::LoopControlStatement(pTHX_ OP *p_op, AST::Term *kid)
  : Term(p_op, pj_ttype_loop_control), jump_target(NULL)
{
  if (kid != NULL)
    kids.push_back(kid);

  switch (p_op->op_type) {
  case OP_NEXT:
    ctl_type = pj_lctl_next;
    break;
  case OP_REDO:
    ctl_type = pj_lctl_redo;
    break;
  case OP_LAST:
    ctl_type = pj_lctl_last;
    break;
  default:
    abort();
  };

  init_label(aTHX);
}

void
LoopControlStatement::init_label(pTHX)
{
  // Alas, it appears that loop control statements can be any of
  // the following:
  // - OP: if bare
  // - PVOP: if using a constant label that has no embedded NUL's
  //         NB: It seems that there is no way to have a label with
  //             embedded NUL's?
  // - SVOP: if using a constant label with embedded NUL's.
  //         NB: Likely, this code path just exists for paranoia!
  // - UNOP: (and OPf_STACKED set) if label is a variable, see example
  //         below. In this case, we cannot compute the jump target
  //         statically. :(
  //
  // Example for dynamic label (*sigh*):
  // $ perl -MO=Concise -E 'last $foo'
  // 5  <@> leave[1 ref] vKP/REFC ->(end)
  // 1     <0> enter ->2
  // 2     <;> nextstate(main 47 -e:1) v:%,{,469764096 ->3
  // 4     <1> last vKS/1 ->5
  // -        <1> ex-rv2sv sK/1 ->4
  // 3           <$> gvsv(*foo) s ->4
  // -e syntax OK

  if (perl_op->op_flags & OPf_STACKED) {
    // The dreaded "it's a dynamic label" case!
    // Nothing much we can do.
    _label_is_dynamic = true;
    label = std::string("");
    _label_is_utf8 = false; // ho-humm...
    _has_label = true;
    return;
  }

  _label_is_dynamic = false;
  // OPf_SPECIAL set means it's a bare loop control statement.
  if (perl_op->op_flags & OPf_SPECIAL) {
    label = std::string("");
    _label_is_dynamic = false;
    _has_label = false;
    _label_is_utf8 = false;
    return;
  }

  // Must be a PVOP
  _has_label = true;
  const char * const label_str = cPVOPx(perl_op)->op_pv;
#ifdef OPpPV_IS_UTF8
  /* 5.16 and up */
  _label_is_utf8 = cPVOPx(perl_op)->op_private & OPpPV_IS_UTF8;
#else
  _label_is_utf8 = false;
#endif
  if (label_str != NULL)
    label = std::string(label_str, strlen(label_str));
  else
    label = std::string("");

  return;
}

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
NumericConstant::dump(int indent_lvl) const
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
StringConstant::dump(int indent_lvl) const
{
  S_dump_tree_indent(indent_lvl);
  printf("C = (string, %s)\"%*s\"\n",
         (is_utf8 ? "UTF8" : "binary"),
         (int)string_value.length(),
         string_value.c_str());
}

void
ArrayConstant::dump(int indent_lvl) const
{
  std::string t = get_value_type()->to_string();
  S_dump_tree_indent(indent_lvl);
  printf("C = (array, %s)\n", t.c_str());
}

void
UndefConstant::dump(int indent_lvl) const
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
  case pj_sigil_code:
    return '&';
  }
  abort();
  return '\0';
}

void
VariableDeclaration::dump(int indent_lvl) const
{
  S_dump_tree_indent(indent_lvl);
  printf("VD (%c) = %i", S_sigil_character(sigil), this->ivar);
  if (Type *value_type = get_value_type())
    printf(" : %s", value_type->to_string().c_str());
  printf("\n");
}

Type *Lexical::get_value_type() const
{ return declaration->get_value_type(); }

void Lexical::set_value_type(Type *t)
{ declaration->set_value_type(t); }


void
Lexical::dump(int indent_lvl) const
{
  S_dump_tree_indent(indent_lvl);
  printf("V (%c) = %i", S_sigil_character(declaration->sigil), declaration->ivar);
  if (Type *value_type = get_value_type())
    printf(" : %s", value_type->to_string().c_str());
  printf("\n");
}


void
Global::dump(int indent_lvl) const
{
  S_dump_tree_indent(indent_lvl);
  printf("G = (%c)\n", S_sigil_character(sigil));
}


void
Optree::dump(int indent_lvl) const
{
  S_dump_tree_indent(indent_lvl);
  printf("'UnJITable subtree'\n");
}


void
NullOptree::dump(int indent_lvl) const
{
  printf("'UnJITable nulled subtree'\n");
}

void
Statement::dump(int indent_lvl) const
{
  S_dump_tree_indent(indent_lvl);
  printf("Statement %s:%d\n", CopFILE(cCOPx(perl_op)), CopLINE(cCOPx(perl_op)));
  kids[0]->dump(indent_lvl+1);
}

void
StatementSequence::dump(int indent_lvl) const
{
  S_dump_tree_indent(indent_lvl);
  printf("StatementSequence\n");
  for (size_t i = 0; i < kids.size(); ++i)
    kids[i]->dump(indent_lvl+1);
}

static void
S_dump_op(const PerlJIT::AST::Op *o, const char *op_str, bool is_assignment_form, int indent_lvl)
{
  S_dump_tree_indent(indent_lvl);
  printf(
    "%s '%s%s%s'",
    op_str,
    o->name(),
    is_assignment_form ? "=" : "",
    o->is_integer_variant() ? ", Integer variant" : ""
  );

  if (o->op_class() == pj_opc_baseop) {
    printf("\n");
    return;
  }
  printf(" (\n");
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


void Baseop::dump(int indent_lvl) const
{ S_dump_op(this, "Baseop", false, indent_lvl); }

void Unop::dump(int indent_lvl) const
{ S_dump_op(this, "Unop", false, indent_lvl); }

void Binop::dump(int indent_lvl) const
{ S_dump_op(this, "Binop", is_assignment_form(), indent_lvl); }

void Listop::dump(int indent_lvl) const
{ S_dump_op(this, "Listop", false, indent_lvl); }

void Block::dump(int indent_lvl) const
{ S_dump_op(this, "Block", false, indent_lvl); }

void BareBlock::dump(int indent_lvl) const
{
  S_dump_tree_indent(indent_lvl);
  printf("BareBlock (\n");
  body->dump(indent_lvl + 1);
  if (continuation->get_type() != pj_ttype_empty) {
    S_dump_tree_indent(indent_lvl);
    printf(") Continue (\n");
    continuation->dump(indent_lvl + 1);
  }
  S_dump_tree_indent(indent_lvl);
  printf(")\n");
}

void While::dump(int indent_lvl) const
{
  S_dump_tree_indent(indent_lvl);
  printf("%s%s (\n", evaluate_after ? "Do" : "", negated ? "Until": "While");
  condition->dump(indent_lvl + 2);
  body->dump(indent_lvl + 1);
  if (continuation->get_type() != pj_ttype_empty) {
    S_dump_tree_indent(indent_lvl);
    printf(") Continue (\n");
    continuation->dump(indent_lvl + 1);
  }
  S_dump_tree_indent(indent_lvl);
  printf(")\n");
}

void For::dump(int indent_lvl) const
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

void Foreach::dump(int indent_lvl) const
{
  S_dump_tree_indent(indent_lvl);
  printf("Foreach (\n");
  iterator->dump(indent_lvl + 2);
  expression->dump(indent_lvl + 2);
  body->dump(indent_lvl + 1);
  if (continuation->get_type() != pj_ttype_empty) {
    S_dump_tree_indent(indent_lvl);
    printf(") Continue (\n");
    continuation->dump(indent_lvl + 1);
  }
  S_dump_tree_indent(indent_lvl);
  printf(")\n");
}

void ListTransformation::dump(int indent_lvl) const
{
  S_dump_tree_indent(indent_lvl);
  if (type == pj_ttype_map)
    printf("Map (\n");
  else if (type == pj_ttype_grep)
    printf("Grep (\n");
  else
    abort();
  body->dump(indent_lvl+1);
  parameters->dump(indent_lvl+1);
  S_dump_tree_indent(indent_lvl);
  printf(")\n");
}

void Empty::dump(int indent_lvl) const
{
  S_dump_tree_indent(indent_lvl);
  printf("Empty\n");
}

void List::dump(int indent_lvl) const
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

void
SubCall::dump(int indent_lvl) const
{
  S_dump_tree_indent(indent_lvl);
  printf("SubCall(\n");
  S_dump_tree_indent(indent_lvl+1);
  printf("CV source:\n");
  this->_cv_source->dump(indent_lvl+2);

  S_dump_tree_indent(indent_lvl+1);
  printf("Args:\n");
  const unsigned int n = this->_arguments.size();
  for (unsigned int i = 0; i < n; ++i)
    this->_arguments[i]->dump(indent_lvl+2);

  S_dump_tree_indent(indent_lvl);
  printf(")\n");
}

void
MethodCall::dump(int indent_lvl) const
{
  S_dump_tree_indent(indent_lvl);
  printf("MethodCall(\n");
  S_dump_tree_indent(indent_lvl+1);
  printf("CV source:\n");
  this->get_cv_source()->dump(indent_lvl+2);

  S_dump_tree_indent(indent_lvl+1);
  printf("Invocant:\n");
  this->_invocant->dump(indent_lvl+2);

  S_dump_tree_indent(indent_lvl+1);
  printf("Args:\n");
  const unsigned int n = this->get_arguments().size();
  for (unsigned int i = 0; i < n; ++i)
    this->get_arguments()[i]->dump(indent_lvl+2);

  S_dump_tree_indent(indent_lvl);
  printf(")\n");
}

void LoopControlStatement::dump(int indent_lvl) const
{
  static int recursive = 0;

  S_dump_tree_indent(indent_lvl);
  std::string name;
  const char *target_unresolved = (jump_target == NULL)
                                  ? " (jump target unresolved!) "
                                  : "";

  switch (ctl_type) {
  case pj_lctl_next:
    name = "Next";
    break;
  case pj_lctl_redo:
    name = "Redo";
    break;
  case pj_lctl_last:
    name = "Last";
    break;
  default:
    abort(); // shouldn't happen!
  }

  if (label_is_dynamic()) {
    printf("%s%s(\n", name.c_str(), target_unresolved);
    get_kids()[0]->dump(indent_lvl+1);
    S_dump_tree_indent(indent_lvl);
    printf(")\n");
  }
  else {
    if (has_label()) {
      printf(
        "%s '%s'%s\n",
        name.c_str(),
        get_label().c_str(),
        target_unresolved
      );
    }
    else {
      printf("%s%s\n", name.c_str(), target_unresolved);
    }
  }
  if (jump_target && !recursive) {
    recursive = 1;
    jump_target->dump(indent_lvl+10);
    recursive = 0;
  }
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

pj_term_type Term::get_type() const
{ return type; }

void Term::set_type(const pj_term_type t)
{ type = t; }

OP * Term::get_perl_op() const
{ return perl_op; }

void Term::set_perl_op(OP *p_op)
{ perl_op = p_op; }

Type *Term::get_value_type() const
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

OP *Term::first_op()
{
  return perl_op;
}

OP *Term::last_op()
{
  return perl_op;
}

void Term::set_value_type(Type *t)
{
  delete _value_type;
  _value_type = t;
}

pj_op_context Term::context() const
{
  return pj_op_context(OP_GIMME(perl_op, pj_context_caller));
}

const char *
Op::name() const
{
  return pj_ast_op_names[op_type];
}

unsigned int Op::flags() const
{ return pj_ast_op_flags[op_type]; }

pj_op_type Op::get_op_type() const
{ return op_type; }

void Op::set_op_type(pj_op_type t)
{ op_type = t; }

bool Op::is_integer_variant() const
{ return _is_integer_variant; }

void Op::set_integer_variant(bool is_integer_variant)
{ _is_integer_variant = is_integer_variant; }

bool
Binop::is_assignment_form() const
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
  return op_type == pj_binop_sassign && kids[1]->get_perl_op() == perl_op;
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

std::vector<PerlJIT::AST::Term *> BareBlock::get_kids() const
{
  std::vector<PerlJIT::AST::Term *> kids;

  kids.push_back(body);
  if (continuation->get_type() != pj_ttype_empty)
    kids.push_back(continuation);

  return kids;
}

std::vector<PerlJIT::AST::Term *> While::get_kids() const
{
  std::vector<PerlJIT::AST::Term *> kids;

  kids.push_back(condition);
  kids.push_back(body);
  if (continuation->get_type() != pj_ttype_empty)
    kids.push_back(continuation);

  return kids;
}

OP *For::last_op()
{
  if (init->get_type() == pj_ttype_empty)
    return perl_op;

  // handle the case when the unstack op has been detached from the tree
  OP *leave = init->get_perl_op()->op_sibling;
  if (leave->op_type == OP_UNSTACK)
    leave = leave->op_sibling;

  assert(leave->op_type == OP_LEAVELOOP);
  return leave;
}

std::vector<PerlJIT::AST::Term *> For::get_kids() const
{
  std::vector<PerlJIT::AST::Term *> kids;

  kids.push_back(init);
  kids.push_back(condition);
  kids.push_back(step);
  kids.push_back(body);

  return kids;
}

std::vector<PerlJIT::AST::Term *> Foreach::get_kids() const
{
  std::vector<PerlJIT::AST::Term *> kids;

  kids.push_back(iterator);
  kids.push_back(expression);
  kids.push_back(body);
  kids.push_back(continuation);

  return kids;
}

std::vector<PerlJIT::AST::Term *> ListTransformation::get_kids()
{
  std::vector<PerlJIT::AST::Term *> kids;

  kids.push_back(body);
  kids.push_back(parameters);

  return kids;
}
