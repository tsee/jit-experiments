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
#include "src/pj_ast_op_switch-gen.inc"
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
      : containing_cv(cv), last_nextstate(NULL), current_sequence(NULL)
    {
      // typed_declarations may end up being NULL!
      if (cv != NULL)
        typed_declarations = pj_get_typed_variable_declarations(aTHX_ cv);
    }

    visit_control_t
    visit_op(pTHX_ OP *o, OP *parentop)
    {
      unsigned int otype;
      otype = o->op_type;

      if (otype == OP_NEXTSTATE) {
        last_nextstate = o;
        return VISIT_SKIP;
      }

      PJ_DEBUG_1("Considering %s\n", OP_NAME(o));

      /* Attempt JIT if the right OP type. Don't recurse if so. */
      if (IS_AST_COMPATIBLE_ROOT_OP_TYPE(otype)) {
        PerlJIT::AST::Term *ast = pj_attempt_jit(aTHX_ o, *this);
        if (ast) {
          if (last_nextstate && last_nextstate->op_sibling == o)
            create_statement(ast);
          else
            candidates.push_back(ast);
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
        reference->op_type == OP_PADSV ? pj_sigil_scalar :
        reference->op_type == OP_PADAV ? pj_sigil_array :
        reference->op_type == OP_PADHV ? pj_sigil_hash :
                                         (pj_variable_sigil) -1;
      if (sigil == (pj_variable_sigil) -1)
        croak("Unrecognized sigil");

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
    AST::StatementSequence *current_sequence;
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
      PJ_DEBUG_2("pj_build_ast considering kid (%u) type %s\n", ikid, OP_NAME(kid));
      if (PJ_DEBUGGING && kid->op_type == OP_NULL) {
        printf("             kid is OP_NULL and used to be %s\n", PL_op_name[kid->op_targ]);
      }

      if (kid->op_type == OP_NULL && !(kid->op_flags & OPf_KIDS)) {
        PJ_DEBUG_1("Skipping kid (%u) since it's an OP_NULL without kids.\n", ikid);
        continue;
      }

      // FIXME possibly wrong. PUSHMARK assumed to be an implementation detail that is not
      //       strictly necessary in an AST listop. Totally speculative.
      if (kid->op_type == OP_PUSHMARK && !(kid->op_flags & OPf_KIDS)) {
        PJ_DEBUG_1("Skipping kid (%u) since it's an OP_PUSHMARK without kids.\n", ikid);
        continue;
      }

      AST::Term *kid_term = pj_build_ast(aTHX_ kid, visitor);

      // Handle a few special kid cases
      if (kid_term == NULL) {
        // Failed to build sub-AST, free ASTs build thus far before bailing
        PJ_DEBUG("Failed to build sub-AST - unwinding.\n");
        pj_free_term_vector(aTHX_ kid_terms);
        return 1;
      }
      else if (kid_term->type == pj_ttype_op && ((AST::Op *)kid_term)->optype == pj_unop_empty) {
        // empty list is not really a kid, don't include in child list
        delete kid_term;
      }
      else {
        kid_terms.push_back(kid_term);
      }

      if (PJ_DEBUGGING)
        printf("pj_build_ast got kid (%u, %p) of type %s in return\n", ikid, kid_term, kid_term->perl_class());
      ++ikid;
    } // end for kids
  } // end if have kids
  return 0;
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

#define EMIT_UNOP_CODE(perl_op_type, pj_op_type)              \
  case perl_op_type: {                                        \
      MAKE_DEFAULT_KID_VECTOR                                 \
      assert(kid_terms.size() == 1);                          \
      retval = new AST::Unop(o, pj_op_type, kid_terms[0]);    \
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
      break;                                                  \
    }

#define EMIT_BINOP_CODE(perl_op_type, pj_op_type)                         \
  case perl_op_type: {                                                    \
      MAKE_DEFAULT_KID_VECTOR                                             \
      assert(kid_terms.size() == 2);                                      \
      retval = new AST::Binop(o, pj_op_type, kid_terms[0], kid_terms[1]); \
      break;                                                              \
    }

#define EMIT_BINOP_CODE_OPTIONAL(perl_op_type, pj_op_type)                \
  case perl_op_type: {                                                    \
      MAKE_DEFAULT_KID_VECTOR                                             \
      assert(kid_terms.size() <= 2);                                      \
      if (kid_terms.size() < 2)                                           \
        kid_terms.push_back(NULL);                                        \
      retval = new AST::Binop(o, pj_op_type, kid_terms[0], kid_terms[1]); \
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

  case OP_NULL: {
      MAKE_DEFAULT_KID_VECTOR
      if (kid_terms.size() == 1) {
        if (o->op_targ == 0) {
          // attempt to pass through this untyped null-op. FIXME likely WRONG
          PJ_DEBUG("Passing through kid of OP_NULL\n");
          retval = kid_terms[0];
        }
        else {
          const unsigned int targ_otype = (unsigned int)o->op_targ;
          switch (targ_otype) {
          case OP_RV2SV: // Skip into ex-rv2sv for optimized global scalar access
            PJ_DEBUG("Passing through kid of ex-rv2sv\n");
            retval = kid_terms[0];
            break;
          default:
            PJ_DEBUG_1("Cannot represent this NULL OP with AST. Emitting OP tree term in AST. (%s)", OP_NAME(o));
            pj_find_jit_candidates_internal(aTHX_ o, visitor);
            retval = new AST::Optree(o);
            pj_free_term_vector(aTHX_ kid_terms);
            break;
          }
        }
      }
      else {
        PJ_DEBUG_1("Cannot represent this NULL OP with AST. Emitting OP tree term in AST. (%s)", OP_NAME(o));
        pj_find_jit_candidates_internal(aTHX_ o, visitor);
        retval = new AST::Optree(o);
        pj_free_term_vector(aTHX_ kid_terms);
      }
      break;
    }

  case OP_ANDASSIGN:
  case OP_ORASSIGN:
  case OP_DORASSIGN: {
      //  6        <|> orassign(other->7) vK/1 ->9
      //  5           <0> padsv[$x:1,2] sRM ->6
      //  8           <1> sassign sK/BKWARD,1 ->9
      //  7              <$> const[IV 123] s ->8
      // Patch out the sassign!
      MAKE_DEFAULT_KID_VECTOR
      assert(kid_terms.size() == 2);
      if (kid_terms[1]->type == pj_ttype_op
          && ((AST::Op *)kid_terms[1])->optype == pj_binop_sassign
          && ((AST::Op *)kid_terms[1])->kids[1] == NULL)
      {
        // one of them funny sassigns...
        PJ_DEBUG("Patching out uninteresting sassign without and/or/dor-assign.\n");
        AST::Binop *tmp = (AST::Binop *)kid_terms[1];
        kid_terms[1] = tmp->kids[0];
        tmp->kids[0] = NULL; // ownership fix
        delete tmp;
      }
      else {
        kid_terms[1]->dump();
        abort();
      }
      pj_op_type t =   otype == OP_ANDASSIGN ? pj_binop_bool_and
                     : otype == OP_ORASSIGN  ? pj_binop_bool_or
                     :                         pj_binop_definedor;
      retval = new AST::Binop(o, t, kid_terms[0], kid_terms[1]);
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
          retval = new AST::Unop(o, pj_unop_empty, NULL);
        }
      }
      else { // undecidable yet
        retval = new AST::Unop(o, pj_unop_empty, NULL);
      }
      break;
    }

  case OP_LIST: {
      MAKE_DEFAULT_KID_VECTOR
      if (kid_terms.size() == 1)
        retval = kid_terms[0];
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

  // Special cases, not auto-generated
    EMIT_LISTOP_CODE(OP_SCHOP, pj_listop_chop)
    EMIT_LISTOP_CODE(OP_SCHOMP, pj_listop_chomp)

// Include auto-generated OP case list using the EMIT_*_CODE* macros
#include "pj_ast_optree_emit-gen.inc"

  default:
    warn("Shouldn't happen! Unsupported OP!? %s\n", OP_NAME(o));
    abort();
  }

#undef MAKE_DEFAULT_KID_VECTOR
#undef EMIT_BINOP_CODE
#undef EMIT_BINOP_CODE_OPTIONAL
#undef EMIT_UNOP_CODE
#undef EMIT_UNOP_CODE_OPTIONAL
#undef EMIT_LISTOP_CODE

  /* PMOP doesn't matter for JIT right now */
  /*
    if (o && OP_CLASS(o) == OA_PMOP && o->op_type != OP_PUSHRE
          && (kid = PMOP_pmreplroot(cPMOPo)))
    {}
  */

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
