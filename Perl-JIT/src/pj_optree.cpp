#include "pj_optree.h"
#include <stdlib.h>
#include <stdio.h>

#include <OPTreeVisitor.h>

#include "ppport.h"
#include "pj_inline.h"
#include "pj_debug.h"

#include "pj_ast_terms.h"
#include "pj_global_state.h"
#include "pj_keyword_plugin.h"

#include <vector>
#include <string>
#include <tr1/unordered_map>

/* inspired by B.xs */
#define PMOP_pmreplstart(o)	o->op_pmstashstartu.op_pmreplstart
#define PMOP_pmreplroot(o)	o->op_pmreplrootu.op_pmreplroot

/* From B::Generate */
#ifndef PadARRAY
# if PERL_VERSION < 8 || (PERL_VERSION == 8 && !PERL_SUBVERSION)
typedef AV PADLIST;
typedef AV PAD;
# endif
# define PadlistARRAY(pl)	((PAD **)AvARRAY(pl))
# define PadlistNAMES(pl)	(*PadlistARRAY(pl))
# define PadARRAY		AvARRAY
#endif

// Should never matter, just to get the "permissible OPs" list to be
// insensitive to not knowing OP_AELEMFAST_LEX on older perls.
#if PERL_VERSION <= 14
# define OP_AELEMFAST_LEX -1
#endif

namespace PerlJIT {
  class OPTreeJITCandidateFinder;
}

using namespace PerlJIT;
using namespace std::tr1;
using namespace std;

static vector<PerlJIT::AST::Term *>
pj_find_jit_candidates_internal(pTHX_ OP *o, OPTreeJITCandidateFinder &visitor);
static PerlJIT::AST::Term *
pj_attempt_jit(pTHX_ OP *o, OPTreeJITCandidateFinder &visitor);
static PerlJIT::AST::Term *
pj_build_ast(pTHX_ OP *o, OPTreeJITCandidateFinder &visitor);


// This will import two macros IS_AST_COMPATIBLE_ROOT_OP_TYPE
// and IS_AST_COMPATIBLE_OP_TYPE. The macros determine whether to attempt
// to ASTify a given Perl OP. Difference between the two macros described
// below.
#include "pj_ast_op_switch-gen.inc"
/* AND and OR at top level can be used in "interesting" places such as looping constructs.
 * Thus, we'll -- for now -- only support them as OPs within a tree.
 * NULLs may need to be skipped occasionally, so we do something similar.
 * PADSVs are recognized as subtrees now, so no use making them jittable root OP.
 * CONSTs would be further constant folded if they were a candidate root OP, so
 * no sense trying to JIT them if they're free-standing. */

namespace PerlJIT {
  class OPTreeJITCandidateFinder : public OPTreeVisitor
  {
  public:
    OPTreeJITCandidateFinder(pTHX_ CV *cv)
      : containing_cv(cv), last_nextstate(NULL), current_sequence(NULL),
        skip_next_leaveloop(false)
    {
      // typed_declarations may end up being NULL!
      if (cv != NULL)
        typed_declarations = pj_get_typed_variable_declarations(aTHX_ cv);
    }

    visit_control_t
    visit_op(pTHX_ OP *o, OP *parentop)
    {
      unsigned int otype = o->op_type;
      bool astify_kid = false;

      // handles top-level do{} blocks and if()/unless() statements; can
      // be removed when removing the rest of the code handling statement
      // sequences
      if (otype == OP_NULL && (o->op_flags & OPf_KIDS)) {
        unsigned int ktype = cUNOPo->op_first->op_type;

        astify_kid = ktype == OP_SCOPE;
      }

      if (otype == OP_NEXTSTATE) {
        last_nextstate = o;
        return VISIT_SKIP;
      }

      // C-style for loops with initializer are emitted as the sequence
      // nextstate -> initializer -> unstack -> leaveloop
      //
      // this is handled in pj_build_ast() by processing the unstack/leaveloop
      // after ASTifying the initializer, so here we skip the loop body
      // if it was already processed
      if (otype == OP_LEAVELOOP && skip_next_leaveloop) {
        skip_next_leaveloop = false;
        return VISIT_SKIP;
      }

      PJ_DEBUG_1("Considering %s\n", OP_NAME(o));

      /* Attempt JIT if the right OP type. Don't recurse if so. */
      if (IS_AST_COMPATIBLE_ROOT_OP_TYPE(otype) || astify_kid) {
        OP *source = astify_kid ? cUNOPo->op_first : o;
        PerlJIT::AST::Term *ast = pj_attempt_jit(aTHX_ source, *this);
        if (ast) {
          if (last_nextstate && last_nextstate->op_sibling == o)
            create_statement(ast);
          else
            candidates.push_back(ast);
          if (otype != OP_LEAVELOOP && ast->type == pj_ttype_for)
            skip_next_leaveloop = true;
          last_nextstate = NULL;
        }
        return VISIT_SKIP;
      }
      return VISIT_CONT;
    } // end 'visit_op'

    void
    create_statement(AST::Term *expression)
    {
      AST::Statement *stmt = new AST::Statement(last_nextstate, expression);

      if (!current_sequence ||
          !current_sequence->kids.back()->perl_op->op_sibling ||
          current_sequence->kids.back()->perl_op->op_sibling->op_sibling != last_nextstate) {
        current_sequence = new AST::StatementSequence();
        candidates.push_back(current_sequence);
      }

      current_sequence->kids.push_back(stmt);
    }

    // Declaration points to an op with OPpLVAL_INTRO, reference points
    // the op that is being processed (which might or might not have the
    // OPpLVAL_INTRO flag).
    // This is required because we might process a variable reference
    // before seeing the declaration.
    AST::VariableDeclaration *
    get_declaration(OP *declaration, OP *reference)
    {
      AST::VariableDeclaration *decl = variables[reference->op_targ];
      if (decl)
        return decl;

      // Use type from CV's MAGIC annotation to tag VariableDeclaration here
      // or otherwise create default type
      pj_variable_sigil sigil =
        reference->op_type == OP_PADSV         ? pj_sigil_scalar :
        reference->op_type == OP_ENTERITER     ? pj_sigil_scalar :
        reference->op_type == OP_PADAV         ? pj_sigil_array :
        reference->op_type == OP_AELEMFAST     ? pj_sigil_array :
        reference->op_type == OP_AELEMFAST_LEX ? pj_sigil_array :
        reference->op_type == OP_PADHV         ? pj_sigil_hash :
                                                 (pj_variable_sigil) -1;
      if (sigil == (pj_variable_sigil) -1) {
        if (PL_opargs[reference->op_type] & OA_TARGLEX)
          sigil = pj_sigil_scalar;
        else
          croak("Unrecognized sigil");
      }

      decl = new AST::VariableDeclaration(declaration, variables.size(), sigil);
      if (typed_declarations) {
        pj_declaration_map_t::iterator it = typed_declarations->find(reference->op_targ);

        if (it != typed_declarations->end())
          set_declaration_type(decl, it->second.get_type());
      }
      if (!decl->get_value_type())
        set_declaration_type(decl, new AST::Scalar(pj_unspecified_type));

      variables[reference->op_targ] = decl;

      return decl;
    }

    void
    set_declaration_type(AST::VariableDeclaration *decl, AST::Type *type)
    {
      if (type->is_scalar()) {
        if (decl->sigil == pj_sigil_array)
          decl->set_value_type(new AST::Array(type));
        else if (decl->sigil == pj_sigil_hash)
          decl->set_value_type(new AST::Hash(type));
        else
          decl->set_value_type(type);
      }
      else if ((type->is_array() && decl->sigil == pj_sigil_array) ||
               (type->is_hash() && decl->sigil == pj_sigil_hash)) {
          decl->set_value_type(type);
      }
      else {
        const char *variable_type =
          decl->sigil == pj_sigil_scalar ? "scalar" :
          decl->sigil == pj_sigil_array  ? "array" :
                                           "hash";
        croak("Declared type %s does not match variable type %s",
              type->to_string().c_str(), variable_type);
      }
    }

    vector<PerlJIT::AST::Term *> candidates;
    unordered_map<PADOFFSET, AST::VariableDeclaration *> variables;
    CV *containing_cv;
    pj_declaration_map_t *typed_declarations;
    OP *last_nextstate;
  private:
    AST::StatementSequence *current_sequence;
    bool skip_next_leaveloop;
  }; // end class OPTreeJITCandidateFinder
}


static void
pj_free_term_vector(pTHX_ vector<PerlJIT::AST::Term *> &kids)
{
  vector<PerlJIT::AST::Term *>::iterator it = kids.begin();
  for (; it != kids.end(); ++it) {
    if (*it != NULL)
      delete *it;
  }
}


static int
pj_build_kid_terms(pTHX_ OP *o, OPTreeJITCandidateFinder &visitor, vector<AST::Term *> &kid_terms)
{
  unsigned int ikid = 0;
  if (o->op_flags & OPf_KIDS) {
    for (OP *kid = ((UNOP*)o)->op_first; kid; kid = kid->op_sibling) {
      PJ_DEBUG_2("pj_build_kid_terms considering kid (%u) type %s\n", ikid, OP_NAME(kid));
      if (PJ_DEBUGGING && kid->op_type == OP_NULL) {
        printf("                   kid is OP_NULL and used to be %s\n", PL_op_name[kid->op_targ]);
      }

      if (kid->op_type == OP_NULL && !(kid->op_flags & OPf_KIDS)) {
        PJ_DEBUG_1("pj_build_kid_terms skipping kid (%u) since it's an OP_NULL without kids.\n", ikid);
        continue;
      }

#if PERL_VERSION >= 18
      // The padrange optimization leaves the OP_PADxV ops in place,
      // so we can just skip it and continue parsing the remaining
      // kids
      if (kid->op_type == OP_PADRANGE) {
        PJ_DEBUG_1("pj_build_kid_terms skipping kid (%u) since it's an OP_PADRANGE.\n", ikid);
        continue;
      }
#endif

      // FIXME possibly wrong. PUSHMARK assumed to be an implementation detail that is not
      //       strictly necessary in an AST listop. Totally speculative.
      if (kid->op_type == OP_PUSHMARK && !(kid->op_flags & OPf_KIDS)) {
        PJ_DEBUG_1("pj_build_kid_terms skipping kid (%u) since it's an OP_PUSHMARK without kids.\n", ikid);
        continue;
      }

      AST::Term *kid_term = pj_build_ast(aTHX_ kid, visitor);

      // Handle a few special kid cases
      if (kid_term == NULL) {
        // Failed to build sub-AST, free ASTs build thus far before bailing
        PJ_DEBUG("pj_build_kid_terms failed to build sub-AST - unwinding.\n");
        pj_free_term_vector(aTHX_ kid_terms);
        return 1;
      }
      else if (kid_term->type == pj_ttype_op && ((AST::Op *)kid_term)->optype == pj_baseop_empty) {
        // empty list is not really a kid, don't include in child list
        delete kid_term;
        kid_term = NULL;
      }
      else {
        kid_terms.push_back(kid_term);
      }

      if (PJ_DEBUGGING && kid_term)
        printf("pj_build_kid_terms got kid (%u, %p) of type %s in return\n", ikid, kid_term, kid_term->perl_class());
      ++ikid;
    } // end for kids
  } // end if have kids
  return 0;
}

static PerlJIT::AST::Term *
pj_build_targmy_assignment(PerlJIT::AST::Term *term, OPTreeJITCandidateFinder &visitor)
{
  OP *o = term->perl_op;

  if (!(PL_opargs[o->op_type] & OA_TARGLEX))
    return term;
  if (!(o->op_private & OPpTARGET_MY))
    return term;

  AST::Lexical *var = new AST::Lexical(o, visitor.get_declaration(0, o));

  return new AST::Binop(o, pj_binop_sassign, var, term);
}

static PerlJIT::AST::Term *
pj_build_block_or_term(pTHX_ OP *start, OPTreeJITCandidateFinder &visitor)
{
  if (start->op_type != OP_NEXTSTATE &&
      (start->op_type != OP_NULL || start->op_flags & OPf_KIDS))
    return pj_build_ast(aTHX_ start, visitor);

  AST::StatementSequence *seq = new AST::StatementSequence();

  // Consider this:
  // perl author_tools/jit_ast_dump.pl -c -e '$x = do {1;1;1;1;1}'
  // That emits a chain of
  //   nextstate->null->null->nextstate->...->nextstate->const
  // But we really just want nextstate->const, so do all the skipping
  // over in here.
  while (start && start->op_type == OP_NEXTSTATE && start->op_sibling) {
    OP *nextstate = start;
    start = start->op_sibling;
    while (start && ((start->op_type == OP_NULL &&
                      !(start->op_flags & OPf_KIDS)) ||
                     start->op_type == OP_NEXTSTATE ||
                     start->op_type == OP_UNSTACK))
    {
      // If we find more nextstates connected with nulls, then use
      // the last nextstate in the sequence.
      if (start->op_type == OP_NEXTSTATE)
        nextstate = start;
      start = start->op_sibling;
    }
    if (!start)
      break;
    AST::Term *expression = pj_build_ast(aTHX_ start, visitor);
    if (expression->type == pj_ttype_empty)
      continue;
    AST::Statement *stmt = new AST::Statement(nextstate, expression);

    seq->kids.push_back(stmt);
    start = start->op_sibling;
  }

  if (seq->kids.size())
    return seq;

  // empty sequence, return an empty statement
  delete seq;
  return new AST::Empty();
}

static PerlJIT::AST::Term *
pj_build_body(pTHX_ OP *body, OPTreeJITCandidateFinder &visitor)
{
  if (!body)
    return new PerlJIT::AST::Empty();

  if (body->op_type == OP_SCOPE) {
    if (cUNOPx(body)->op_first->op_type != OP_STUB) {
      OP *first = cUNOPx(body)->op_first;

      if (first->op_type == OP_NULL && first->op_targ == OP_NEXTSTATE)
        first = first->op_sibling;
      return pj_build_block_or_term(aTHX_ first, visitor);
    }
  } else if (body->op_type == OP_LEAVE) {
    return pj_build_block_or_term(aTHX_ body, visitor);
  } else if (body->op_type != OP_STUB)
    return pj_build_block_or_term(aTHX_ body, visitor);

  return new PerlJIT::AST::Empty();
}

static PerlJIT::AST::For *
pj_build_for(pTHX_ OP *start, PerlJIT::AST::Term *init, LOGOP *condition, OP *step, OP *body, OPTreeJITCandidateFinder &visitor)
{
  PerlJIT::AST::Term *ast_condition = NULL, *ast_step = NULL, *ast_body = NULL;
  OP *start_op = init ? init->perl_op : start;
  if (!init)
    init = new PerlJIT::AST::Empty();

  ast_condition = condition ? pj_build_ast(aTHX_ condition->op_first, visitor) :
                              new PerlJIT::AST::Empty();
  ast_body = pj_build_body(aTHX_ body, visitor);
  ast_step = step ? pj_build_ast(aTHX_ step, visitor) :
                    new PerlJIT::AST::Empty();

  return new PerlJIT::AST::For(start_op, init, ast_condition, ast_step, ast_body);
}

static PerlJIT::AST::While *
pj_build_while(pTHX_ OP *start, LOGOP *condition, OP *body, OP *cont, OPTreeJITCandidateFinder &visitor)
{
  PerlJIT::AST::Term *ast_condition = NULL, *ast_body = NULL, *ast_cont = NULL;
  bool is_until = false;
  bool is_do = false;

  if (condition) {
    is_do = condition->op_other == cUNOPx(start)->op_first->op_next;
    if (condition->op_type == OP_OR &&
        condition->op_first->op_type == OP_NULL &&
        condition->op_first->op_targ == OP_NOT) {
      is_until = true;
      ast_condition = pj_build_ast(aTHX_ cUNOPx(condition->op_first)->op_first, visitor);
    } else
      ast_condition = pj_build_ast(aTHX_ condition->op_first, visitor);
  } else
    ast_condition = new PerlJIT::AST::Empty();

  ast_body = pj_build_body(aTHX_ body, visitor);
  ast_cont = pj_build_body(aTHX_ cont, visitor);

  return new PerlJIT::AST::While(start, ast_condition, is_until, is_do,
                                 ast_body, ast_cont);
}

static PerlJIT::AST::BareBlock *
pj_build_block(pTHX_ OP *start, OP *body, OP *cont, OPTreeJITCandidateFinder &visitor)
{
  PerlJIT::AST::Term *ast_body = NULL, *ast_cont = NULL;

  ast_body = pj_build_body(aTHX_ body, visitor);
  ast_cont = pj_build_body(aTHX_ cont, visitor);

  return new PerlJIT::AST::BareBlock(start, ast_body, ast_cont);
}


static AST::Term *
pj_build_foreach(pTHX_ OP *start, OP *body, OP *cont, OPTreeJITCandidateFinder &visitor)
{
  AST::Term *ast_body = NULL, *ast_cont = NULL, *ast_iterator = NULL, *ast_expression = NULL;
  LOOP *enter = cLOOPx(cBINOPx(start)->op_first);
  OP *args = cUNOPx(enter->op_first->op_sibling)->op_first->op_sibling;

  // iterator
  if (enter->op_targ) {
    if (enter->op_private & OPpLVAL_INTRO)
      ast_iterator = visitor.get_declaration((OP *) enter, (OP *) enter);
    else
      ast_iterator = new AST::Lexical((OP *) enter, visitor.get_declaration(0, (OP *) enter));
  } else {
    OP *iterator = enter->op_first->op_sibling->op_sibling;

    if (iterator->op_type == OP_GV)
      ast_iterator = new AST::Global(iterator, pj_sigil_glob);
    else
      ast_iterator = pj_build_ast(aTHX_ iterator, visitor);
  }

  OP *expr = NULL;
  if (enter->op_flags & OPf_STACKED) {
    // handles 'for $a..$b' and 'for @x'
    OP *first = cUNOPx(enter->op_first->op_sibling)->op_first->op_sibling;
    OP *second = first->op_sibling;

    if (second)
      ast_expression = new AST::Binop(enter->op_first->op_sibling, pj_binop_range,
                                      pj_build_ast(aTHX_ first, visitor),
                                      pj_build_ast(aTHX_ second, visitor));
    else
      expr = first;
  } else {
    expr = enter->op_first->op_sibling;
  }

  // skip over optimized-out reverse()
  if (expr && expr->op_type == OP_NULL && expr->op_targ == OP_REVERSE)
    expr = cLISTOPx(expr)->op_first->op_sibling;
  if (!ast_expression)
    ast_expression = pj_build_ast(aTHX_ expr, visitor);

  // add extra node for the optimized-out reverse
  if (enter->op_private & OPpITER_REVERSED) {
    std::vector<AST::Term *> kids;

    kids.resize(1);
    kids[0] = ast_expression;
    ast_expression = new AST::Listop(enter->op_first->op_sibling, pj_listop_reverse, kids);
  }

  ast_body = pj_build_body(aTHX_ body, visitor);
  ast_cont = pj_build_body(aTHX_ cont, visitor);

  return new AST::Foreach(start, ast_iterator, ast_expression, ast_body, ast_cont);
}

static PerlJIT::AST::Term *
pj_build_loop(pTHX_ OP *start, PerlJIT::AST::Term *init, OPTreeJITCandidateFinder &visitor)
{
  LOOP *enter = cLOOPx(cBINOPx(start)->op_first);
  LOGOP *cond = NULL;
  LISTOP *lineseq = NULL;
  OP *cont = NULL, *step = NULL;
  bool has_cond = enter->op_sibling->op_type == OP_NULL;

  if (has_cond) {
    cond = cLOGOPx(cUNOPx(enter->op_sibling)->op_first);
    lineseq = cLISTOPx(cond->op_first->op_sibling);
  } else if (enter->op_sibling->op_type == OP_STUB) {
    return new AST::Empty();
  } else if (enter->op_sibling->op_type == OP_LINESEQ) {
    lineseq = cLISTOPx(enter->op_sibling);
  } else {
    return NULL;
  }

  bool is_loop = lineseq->op_last->op_type == OP_UNSTACK;
  // loops with a single block (for loops w/o a step, while/blocks
  // without a continue have an OP_NEXTSTATE as the kid of lineseq,
  // otherwise they have an OP_LEAVE/OP_ENTER pair or OP_SCOPE
  bool has_one_subexpr = lineseq->op_first->op_type == OP_NEXTSTATE;
  bool is_modifier = lineseq->op_first->op_sibling &&
                     lineseq->op_first->op_sibling->op_type != OP_UNSTACK;
  bool has_continue = false;
  OP *body = lineseq->op_first;

  if (!has_one_subexpr) {
    unsigned int otype = lineseq->op_first->op_sibling->op_type;

    has_continue = otype == OP_LEAVE || otype == OP_SCOPE;
  }

  if (has_continue)
    cont = lineseq->op_first->op_sibling;
  else if (!has_one_subexpr && lineseq->op_first->op_type != OP_STUB &&
           is_modifier)
    step = lineseq->op_first->op_sibling;

  if (init || step)
    return pj_build_for(aTHX_ start, init, cond, step, body, visitor);
  else if (enter->op_type == OP_ENTERITER)
    return pj_build_foreach(aTHX_ start, body, cont, visitor);
  else if (is_loop)
    return pj_build_while(aTHX_ start, cond, body, cont, visitor);
  else
    return pj_build_block(aTHX_ start, body, cont, visitor);
}

/* Walk OP tree recursively, build ASTs, build subtrees */
static PerlJIT::AST::Term *
pj_build_ast(pTHX_ OP *o, OPTreeJITCandidateFinder &visitor)
{
  PerlJIT::AST::Term *retval = NULL;

  assert(o);

  const unsigned int otype = o->op_type;
  PJ_DEBUG_1("pj_build_ast ASTing OP of type %s\n", OP_NAME(o));
  if (!IS_AST_COMPATIBLE_OP_TYPE(otype)) {
    // Can't represent OP with AST. So instead, recursively scan for
    // separate candidates and treat as subtree.
    PJ_DEBUG_1("Cannot represent this OP with AST. Emitting OP tree term in AST (Perl OP=%s).\n", OP_NAME(o));
    retval = new AST::Optree(o);
    pj_find_jit_candidates_internal(aTHX_ o, visitor);
    if (PJ_DEBUGGING)
      retval->dump();
    return retval;
  }

  // Build child list if applicable
  vector<AST::Term *> kid_terms;


#define MAKE_DEFAULT_KID_VECTOR                           \
  vector<AST::Term *> kid_terms;                          \
  if (pj_build_kid_terms(aTHX_ o, visitor, kid_terms)) {  \
    pj_free_term_vector(aTHX_ kid_terms);                 \
    return NULL;                                          \
  }                                                       \

#define EMIT_BASEOP_CODE(perl_op_type, pj_op_type)            \
  case perl_op_type: {                                        \
      MAKE_DEFAULT_KID_VECTOR                                 \
      assert(kid_terms.size() == 0);                          \
      retval = new AST::Baseop(o, pj_op_type);                \
      retval = pj_build_targmy_assignment(retval, visitor);   \
      break;                                                  \
    }

#define EMIT_UNOP_CODE(perl_op_type, pj_op_type)              \
  case perl_op_type: {                                        \
      MAKE_DEFAULT_KID_VECTOR                                 \
      assert(kid_terms.size() == 1);                          \
      retval = new AST::Unop(o, pj_op_type, kid_terms[0]);    \
      retval = pj_build_targmy_assignment(retval, visitor);   \
      break;                                                  \
    }

#define EMIT_UNOP_CODE_OPTIONAL(perl_op_type, pj_op_type)     \
  case perl_op_type: {                                        \
      MAKE_DEFAULT_KID_VECTOR                                 \
      assert(kid_terms.size() == 1 || kid_terms.size() == 0); \
      if (kid_terms.size() == 1)                              \
        retval = new AST::Unop(o, pj_op_type, kid_terms[0]);  \
      else /* no kids */                                      \
        retval = new AST::Unop(o, pj_op_type, NULL);          \
      retval = pj_build_targmy_assignment(retval, visitor);   \
      break;                                                  \
    }

#define EMIT_UNOP_INTEGER_CODE(perl_op_type, pj_op_type)      \
  case perl_op_type: {                                        \
      MAKE_DEFAULT_KID_VECTOR                                 \
      assert(kid_terms.size() == 1 || kid_terms.size() == 0); \
      if (kid_terms.size() == 1)                              \
        retval = new AST::Unop(o, pj_op_type, kid_terms[0]);  \
      else /* no kids */                                      \
        retval = new AST::Unop(o, pj_op_type, NULL);          \
      ((AST::Op *)retval)->set_integer_variant(true);         \
      retval = pj_build_targmy_assignment(retval, visitor);   \
      break;                                                  \
    }

#define EMIT_BINOP_CODE(perl_op_type, pj_op_type)                         \
  case perl_op_type: {                                                    \
      MAKE_DEFAULT_KID_VECTOR                                             \
      assert(kid_terms.size() == 2);                                      \
      retval = new AST::Binop(o, pj_op_type, kid_terms[0], kid_terms[1]); \
      retval = pj_build_targmy_assignment(retval, visitor);               \
      break;                                                              \
    }

#define EMIT_BINOP_INTEGER_CODE(perl_op_type, pj_op_type)                 \
  case perl_op_type: {                                                    \
      MAKE_DEFAULT_KID_VECTOR                                             \
      assert(kid_terms.size() == 2);                                      \
      retval = new AST::Binop(o, pj_op_type, kid_terms[0], kid_terms[1]); \
      ((AST::Op *)retval)->set_integer_variant(true);                     \
      retval = pj_build_targmy_assignment(retval, visitor);               \
      break;                                                              \
    }

#define EMIT_BINOP_CODE_OPTIONAL(perl_op_type, pj_op_type)                \
  case perl_op_type: {                                                    \
      MAKE_DEFAULT_KID_VECTOR                                             \
      assert(kid_terms.size() <= 2);                                      \
      if (kid_terms.size() < 2)                                           \
        kid_terms.push_back(NULL);                                        \
      retval = new AST::Binop(o, pj_op_type, kid_terms[0], kid_terms[1]); \
      retval = pj_build_targmy_assignment(retval, visitor);               \
      break;                                                              \
    }

#define EMIT_LISTOP_CODE(perl_op_type, pj_op_type)        \
  case perl_op_type: {                                    \
      MAKE_DEFAULT_KID_VECTOR                             \
      retval = new AST::Listop(o, pj_op_type, kid_terms); \
      break;                                              \
    }

  switch (otype) {
  case OP_CONST: {
      // FIXME OP_CONST can also be who-knows-what-else
      SV *constsv = cSVOPx_sv(o);
      if (SvIOK(constsv)) {
        retval = new AST::NumericConstant(o, (IV)SvIV(constsv));
      }
      else if (SvUOK(constsv)) {
        retval = new AST::NumericConstant(o, (UV)SvUV(constsv));
      }
      else if (SvNOK(constsv)) {
        retval = new AST::NumericConstant(o, (NV)SvNV(constsv));
      }
      else if (SvPOK(constsv)) {
        retval = new AST::StringConstant(aTHX_ o, constsv);
      }
      else { // FAIL. Cast to NV
        if (PJ_DEBUGGING) {
          PJ_DEBUG("Casting OP_CONST's SV to an NV since type is unclear. SV dump follows:");
          sv_dump(constsv);
        }
        retval = new AST::NumericConstant(o, (NV)SvNV(constsv));
      }

      break;
    }

  case OP_PADSV:
  case OP_PADAV:
  case OP_PADHV:
    if (o->op_private & OPpLVAL_INTRO)
      retval = visitor.get_declaration(o, o);
    else
      retval = new AST::Lexical(o, visitor.get_declaration(0, o));
    break;

  case OP_GVSV:
    // FIXME OP_GVSV with OPpLVAL_INTRO is "local $x"
    retval = new AST::Global(o, pj_sigil_scalar);
    pj_free_term_vector(aTHX_ kid_terms);
    break;

  case OP_RV2AV: {
      if (cUNOPo->op_first->op_type == OP_GV)
        retval = new AST::Global(o, pj_sigil_array);
      else
        retval = new AST::Unop(o, pj_unop_av_deref, pj_build_ast(aTHX_ cUNOPo->op_first, visitor));

      break;
    }

  case OP_RV2HV: {
      if (cUNOPo->op_first->op_type == OP_GV)
        retval = new AST::Global(o, pj_sigil_hash);
      else
        retval = new AST::Unop(o, pj_unop_hv_deref, pj_build_ast(aTHX_ cUNOPo->op_first, visitor));

      break;
    }

  case OP_RV2GV: {
      if (cUNOPo->op_first->op_type == OP_GV)
        retval = new AST::Global(o, pj_sigil_glob);
      else
        retval = new AST::Unop(o, pj_unop_gv_deref, pj_build_ast(aTHX_ cUNOPo->op_first, visitor));

      break;
    }

  case OP_REPEAT: {
      MAKE_DEFAULT_KID_VECTOR
      if (kid_terms.size() == 1) {
        // One list. Weird edge case in Perl (IMO). Move repetition out.
        assert(kid_terms[0]->type == pj_ttype_list);
        AST::List *l = (AST::List *)kid_terms[0];
        assert(l->kids.size() >= 2);
        AST::Term *rep = l->kids.back();
        l->kids.pop_back();
        kid_terms.push_back(rep);
      }
      retval = new AST::Listop(o, pj_listop_repeat, kid_terms);
      break;
    }

  case OP_NULL: {
      const unsigned int targ_otype = (unsigned int)o->op_targ;
      MAKE_DEFAULT_KID_VECTOR
      if (targ_otype == OP_AELEM) {
        // AELEMFASTified aelem!
        PJ_DEBUG("Passing through kid of ex-aelem\n");
        retval = kid_terms[0];
        if (kid_terms.size() > 1)
          delete kid_terms[1];
      }
      else if (targ_otype == OP_LIST) {
        retval = new AST::List(kid_terms);
      }
      else if (kid_terms.size() == 1) {
        if (o->op_targ == 0) {
          // attempt to pass through this untyped null-op. FIXME likely WRONG
          PJ_DEBUG("Passing through kid of OP_NULL\n");
          retval = kid_terms[0];
        }
        else {
          switch (targ_otype) {
          case OP_RV2AV:
          case OP_RV2SV:
            // Skip into ex-rv2sv for optimized global scalar/array access
            PJ_DEBUG("Passing through kid of ex-rv2sv or ex-rv2av\n");
            retval = kid_terms[0];
            break;
          default:
            PJ_DEBUG_1("Cannot represent this NULL OP with AST. Emitting OP tree term in AST. (%s)\n", OP_NAME(o));
            pj_find_jit_candidates_internal(aTHX_ o, visitor);
            retval = new AST::Optree(o);
            pj_free_term_vector(aTHX_ kid_terms);
            break;
          }
        }
      }
      else {
        PJ_DEBUG_1("Cannot represent this NULL OP with AST. Emitting OP tree term in AST. (%s)\n", OP_NAME(o));
        pj_find_jit_candidates_internal(aTHX_ o, visitor);
        retval = new AST::Optree(o);
        pj_free_term_vector(aTHX_ kid_terms);
      }
      break;
    }

#if PERL_VERSION > 14
  case OP_AELEMFAST_LEX:
#endif
  case OP_AELEMFAST: {
      AST::Term *array;
      // Technically, AST::{Global,Lexical} usually point at other OP types,
      // but they just so happen to behave the same when trying to access
      // the information for finding the actual variable to read from.
      // So this is expected to work {citation required}.
#if PERL_VERSION <= 14
      if (o->op_flags & OPf_SPECIAL) {
#else
      if (otype == OP_AELEMFAST_LEX) {
#endif
        // lexical
        array = new AST::Lexical(o, visitor.get_declaration(0, o));
      }
      else {
        // package var
        array = new AST::Global(o, pj_sigil_array);
      }
      // aelemfast trick: embed array index in private flag space m(
      AST::Term *constant = new AST::NumericConstant(o, (IV)o->op_private);
      retval = new AST::Binop(o, pj_binop_aelem, array, constant);
      break;
  }

  case OP_SASSIGN: {
      MAKE_DEFAULT_KID_VECTOR
      assert(kid_terms.size() == 2);
      retval = new AST::Binop(o, pj_binop_sassign, kid_terms[1], kid_terms[0]);
      break;
    }

  case OP_SCOPE: {
      if (!cLOOPo->op_first->op_sibling) {
        retval = new AST::Empty();
      }
      else {
        retval = pj_build_ast(aTHX_ cLOOPo->op_first->op_sibling, visitor);
        if (retval->type != pj_ttype_empty)
          retval = new AST::Block(o, retval);
      }
      break;
    }

  case OP_LEAVE: {
      LOOP *enter = cLOOPx(cBINOPo->op_first);
      OP *start = enter->op_sibling;
      assert(enter->op_type == OP_ENTER);
      // while/until statement modifier, including do {} while
      if (start->op_type == OP_NULL && start->op_flags & OPf_KIDS) {
        UNOP *null = cUNOPx(start);
        if (null->op_first->op_type == OP_AND ||
            null->op_first->op_type == OP_OR) {
          // statement modifier whith condition
          retval = pj_build_loop(aTHX_ o, NULL, visitor);
        } else if (null->op_first->op_type == OP_LINESEQ &&
                   cLISTOPx(null->op_first)->op_first->op_sibling &&
                   cLISTOPx(null->op_first)->op_first->op_sibling->op_type == OP_UNSTACK) {
          // statement modifier whith always-true condition
          retval = pj_build_while(aTHX_ o, NULL, cLISTOPx(null->op_first)->op_first, NULL, visitor);
        }
      }

      // assume a do{} block without modifier
      if (!retval) {
        AST::Term *statements = pj_build_block_or_term(aTHX_ start, visitor);
        retval = new AST::Block(o, statements);
      }

      break;
    }

  case OP_LEAVELOOP:
    retval = pj_build_loop(aTHX_ o, NULL, visitor);
    break;

  case OP_ANDASSIGN:
  case OP_ORASSIGN:
  case OP_DORASSIGN: {
      //  6        <|> orassign(other->7) vK/1 ->9
      //  5           <0> padsv[$x:1,2] sRM ->6
      //  8           <1> sassign sK/BKWARD,1 ->9
      //  7              <$> const[IV 123] s ->8
      // Patch out the sassign!
      BINOP *bo = cBINOPo;
      if (!bo->op_first->op_sibling ||
          bo->op_first->op_sibling->op_type != OP_SASSIGN ||
          !(bo->op_first->op_sibling->op_private & OPpASSIGN_BACKWARDS)) {
        abort();
      }
      AST::Term *left = pj_build_ast(aTHX_ bo->op_first, visitor);
      AST::Term *right = pj_build_ast(aTHX_ cBINOPx(bo->op_first->op_sibling)->op_first, visitor);
      pj_op_type t =   otype == OP_ANDASSIGN ? pj_binop_bool_and
                     : otype == OP_ORASSIGN  ? pj_binop_bool_or
                     :                         pj_binop_definedor;
      retval = new AST::Binop(o, t, left, right);
      ((AST::Binop *)retval)->set_assignment_form(true);
      break;
    }

  case OP_STUB: {
      const int gimme = OP_GIMME(o, 0);
      if (gimme) {
        if (gimme == OPf_WANT_SCALAR) {
          retval = new AST::UndefConstant();
        }
        else { // list or void context
          // FIXME really, empty list
          retval = new AST::Baseop(o, pj_baseop_empty);
        }
      }
      else { // undecidable yet
        retval = new AST::Baseop(o, pj_baseop_empty);
      }
      break;
    }

  case OP_LIST: {
      MAKE_DEFAULT_KID_VECTOR
      if (kid_terms.size() == 1)
        retval = kid_terms[0];
      else if (pj_op_context(OP_GIMME(o, pj_context_caller)) == pj_context_list) {
        // FIXME this can often be flattened into the parent list!?
        retval = new AST::List(kid_terms);
        retval->perl_op = 0; // likely unnecessary, but just in case
      }
      else
        retval = new AST::Listop(o, pj_listop_list2scalar, kid_terms);
      break;
    }

  case OP_LSLICE: {
#ifndef NDEBUG
      assert(o->op_flags & OPf_KIDS);
      // Paranoid: Assert two children
      unsigned int nkids = 0;
      for (OP *kid = ((UNOP*)o)->op_first; kid; kid = kid->op_sibling) {
        ++nkids;
        assert(kid->op_type == OP_LIST || (kid->op_type == OP_NULL && kid->op_targ == OP_LIST));
      }
      assert(nkids == 2);
#endif
      vector<AST::Term *> tmp;
      if (pj_build_kid_terms(aTHX_ ((BINOP *)o)->op_first, visitor, tmp)) {
        pj_free_term_vector(aTHX_ tmp);
        return NULL;
      }
      AST::Term *kid1 = new AST::List(tmp);

      tmp.clear();
      if (pj_build_kid_terms(aTHX_ ((BINOP *)o)->op_last, visitor, tmp)) {
        pj_free_term_vector(aTHX_ tmp);
        delete kid1;
        return NULL;
      }
      retval = new AST::Binop(o, pj_binop_list_slice, kid1, new AST::List(tmp));
      break;
    }

  case OP_ASLICE: {
#ifndef NDEBUG
      {
        assert(o->op_flags & OPf_KIDS);
        // Paranoid: Assert three children: pushmark, list, array
        unsigned int nkids = 0;
        OP *kid = ((UNOP*)o)->op_first;
        assert(kid && kid->op_type == OP_PUSHMARK);
        kid = kid->op_sibling;
        assert(kid);
        kid = kid->op_sibling;
        assert(kid);
        assert(!kid->op_sibling);
      }
#endif

      // slice list; may be a real list op or just a single thing
      OP *kid = ((UNOP *)o)->op_first->op_sibling;
      AST::Term *kid_term = pj_build_ast(aTHX_ kid, visitor);
      if (kid_term->type == pj_ttype_op
          && ((AST::Op *)kid_term)->optype == pj_listop_list2scalar)
      {
        AST::Listop *listop = (AST::Listop *)kid_term;
        AST::List *tmp = new AST::List(listop->kids);
        listop->kids.clear();
        delete kid_term;
        kid_term = (AST::Term *)tmp;
      }

      // array
      AST::Term *kid_term2 = pj_build_ast(aTHX_ kid->op_sibling, visitor);
      retval = new AST::Binop(o, pj_binop_array_slice, kid_term, kid_term2);
      break;
    }

  // Special cases, not auto-generated
    EMIT_LISTOP_CODE(OP_SCHOP, pj_listop_chop)
    EMIT_LISTOP_CODE(OP_SCHOMP, pj_listop_chomp)

    EMIT_BINOP_INTEGER_CODE(OP_I_ADD, pj_binop_add)
    EMIT_BINOP_INTEGER_CODE(OP_I_SUBTRACT, pj_binop_subtract)
    EMIT_BINOP_INTEGER_CODE(OP_I_MULTIPLY, pj_binop_multiply)
    EMIT_BINOP_INTEGER_CODE(OP_I_DIVIDE, pj_binop_divide)
    EMIT_BINOP_INTEGER_CODE(OP_I_MODULO, pj_binop_modulo)
    EMIT_BINOP_INTEGER_CODE(OP_I_EQ, pj_binop_num_eq)
    EMIT_BINOP_INTEGER_CODE(OP_I_NE, pj_binop_num_ne)
    EMIT_BINOP_INTEGER_CODE(OP_I_LT, pj_binop_num_lt)
    EMIT_BINOP_INTEGER_CODE(OP_I_GT, pj_binop_num_gt)
    EMIT_BINOP_INTEGER_CODE(OP_I_LE, pj_binop_num_le)
    EMIT_BINOP_INTEGER_CODE(OP_I_GE, pj_binop_num_ge)
    EMIT_BINOP_INTEGER_CODE(OP_I_NCMP, pj_binop_num_cmp)
    EMIT_UNOP_INTEGER_CODE(OP_I_PREINC, pj_unop_preinc)
    EMIT_UNOP_INTEGER_CODE(OP_I_PREDEC, pj_unop_predec)
    EMIT_UNOP_INTEGER_CODE(OP_I_POSTINC, pj_unop_postinc)
    EMIT_UNOP_INTEGER_CODE(OP_I_POSTDEC, pj_unop_postdec)
    EMIT_UNOP_INTEGER_CODE(OP_I_NEGATE, pj_unop_negate)

// Include auto-generated OP case list using the EMIT_*_CODE* macros
#include "pj_ast_optree_emit-gen.inc"

  default:
    warn("Shouldn't happen! Unsupported OP!? %s\n", OP_NAME(o));
    abort();
  }

#undef MAKE_DEFAULT_KID_VECTOR
#undef EMIT_BINOP_CODE
#undef EMIT_BINOP_INTEGER_CODE
#undef EMIT_BINOP_CODE_OPTIONAL
#undef EMIT_UNOP_CODE
#undef EMIT_UNOP_CODE_OPTIONAL
#undef EMIT_UNOP_INTEGER_CODE
#undef EMIT_LISTOP_CODE

  /* PMOP doesn't matter for JIT right now */
  /*
    if (o && OP_CLASS(o) == OA_PMOP && o->op_type != OP_PUSHRE
          && (kid = PMOP_pmreplroot(cPMOPo)))
    {}
  */

  // special-case for C-style for, see also skip_next_leaveloop
  if (o->op_sibling &&
      o->op_sibling->op_type == OP_UNSTACK &&
      o->op_sibling->op_sibling &&
      o->op_sibling->op_sibling->op_type == OP_LEAVELOOP)
    retval = pj_build_loop(aTHX_ o->op_sibling->op_sibling, retval, visitor);

  if (PJ_DEBUGGING && retval != NULL)
    retval->dump();
  return retval;
}



/* Starting from a candidate for JITing, walk the OP tree to accumulate
 * a subtree that can be replaced with a single JIT OP. */
/* TODO: Needs to walk the OPs, checking whether they qualify. If
 *       not, then that subtree needs to be added to the list of
 *       trees to be executed before executing the JIT OP itself,
 *       so that their return values end up on the stack
 *       (warning: TARG optimizations!). Also needs to record the
 *       kind of OP that includes the unJITable subtree so that
 *       "type context" can be inferred. Needs recurse depth-first,
 *       left-hugging in order to get the sub tree is normal
 *       execution order. */

static PerlJIT::AST::Term *
pj_attempt_jit(pTHX_ OP *o, OPTreeJITCandidateFinder &visitor)
{
  PerlJIT::AST::Term *ast;

  if (PJ_DEBUGGING)
    printf("Attempting JIT on %s (%p, %p)\n", OP_NAME(o), o, o->op_next);

  ast = pj_build_ast(aTHX_ o, visitor);

  return ast;
}

/* Traverse OP tree from o until done OR a candidate for JITing was found.
 * For candidates, invoke JIT attempt and then move on without going into
 * the particular sub-tree; tree walking in OPTreeWalker, actual logic in
 * OPTreeJITCandidateFinder! */
static vector<PerlJIT::AST::Term *>
pj_find_jit_candidates_internal(pTHX_ OP *o, OPTreeJITCandidateFinder &visitor)
{
  visitor.visit(aTHX_ o, NULL);
  return visitor.candidates;
}

vector<PerlJIT::AST::Term *>
pj_find_jit_candidates(pTHX_ SV *coderef)
{
  if (!SvROK(coderef) || SvTYPE(SvRV(coderef)) != SVt_PVCV)
    croak("Need a code reference");
  CV *cv = (CV *) SvRV(coderef);

  ENTER;
  SAVECOMPPAD(); // restores both PL_comppad and PL_curpad

  PL_comppad = PadlistARRAY(CvPADLIST(cv))[1];
  PL_curpad = AvARRAY(PL_comppad);

  OPTreeJITCandidateFinder f(aTHX_ cv);
  vector<PerlJIT::AST::Term *> tmp = pj_find_jit_candidates_internal(aTHX_ CvROOT(cv), f);
  if (PJ_DEBUGGING) {
    printf("%i JIT candidate ASTs:\n", (int)tmp.size());
    for (unsigned int i = 0; i < (unsigned int)tmp.size(); ++i) {
      printf("===========================\n");
      tmp[i]->dump();
    }
    printf("===========================\n");
  }

  LEAVE;

  return tmp;
}
