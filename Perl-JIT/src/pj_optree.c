#include "pj_optree.h"
#include <stdlib.h>
#include <stdio.h>

#include <OPTreeVisitor.h>

#include "ppport.h"
#include "pj_debug.h"

#include "pj_ast_terms.h"
#include "pj_ast_jit.h"

#include "pj_jit_op.h"
#include "pj_global_state.h"
#include <vector>

/* inspired by B.xs */
#define PMOP_pmreplstart(o)	o->op_pmstashstartu.op_pmreplstart
#define PMOP_pmreplroot(o)	o->op_pmreplrootu.op_pmreplroot

namespace PerlJIT {
  class OPTreeJITCandidateFinder;

  // Represents a non-JIT-able subtree below
  class OPWithImposedType {
  public:
    OPWithImposedType(OP *o, const pj_basic_type t)
      : op(o), imposed_type(t) {}

    OP *op;
    pj_basic_type imposed_type;
  };
}

using namespace PerlJIT;
using namespace std;


#define IS_JITTABLE_ROOT_OP_TYPE(otype) \
        ( otype == OP_ADD || otype == OP_SUBTRACT || otype == OP_MULTIPLY || otype == OP_DIVIDE \
          || otype == OP_SIN || otype == OP_COS || otype == OP_SQRT || otype == OP_EXP \
          || otype == OP_LOG || otype == OP_POW || otype == OP_INT || otype == OP_NOT \
          || otype == OP_LEFT_SHIFT || otype == OP_RIGHT_SHIFT /* || otype == OP_COMPLEMENT */ \
          || otype == OP_EQ || otype == OP_COND_EXPR || otype == OP_NEGATE )

/* AND and OR at top level can be used in "interesting" places such as looping constructs.
 * Thus, we'll -- for now -- only support them as OPs within a tree.
 * NULLs may need to be skipped occasionally, so we do something similar.
 * PADSVs are recognized as subtrees now, so no use making them jittable root OP.
 * CONSTs would be further constant folded if they were a candidate root OP, so
 * no sense trying to JIT them if they're free-standing. */
#define IS_JITTABLE_OP_TYPE(otype) \
        (IS_JITTABLE_ROOT_OP_TYPE(otype) \
          || otype == OP_PADSV \
          || otype == OP_CONST \
          || otype == OP_AND \
          || otype == OP_OR \
          || otype == OP_NULL )

/* Scan a section of the OP tree and find whichever OP is
 * going to be executed first. This is done by doing pure
 * left-hugging depth-first traversal. Ignores op_next. */
PJ_STATIC_INLINE OP *
pj_find_first_executed_op(pTHX_ OP *o)
{
  PERL_UNUSED_CONTEXT;
  while (1) {
    if (o->op_flags & OPf_KIDS) {
      o = cUNOPo->op_first;
    }
    else {
      return o;
    }
  }
  /* TODO: handle PMOP? */
  /*
    if (o && OP_CLASS(o) == OA_PMOP && o->op_type != OP_PUSHRE
          && (kid = PMOP_pmreplroot(cPMOPo)))
    {}
  */
  abort(); /* not reached */
}


STATIC void
pj_free_terms_vector(std::vector<pj_term_t *> &terms)
{
  std::vector<pj_term_t *>::iterator it = terms.begin();
  for (; it != terms.end(); ++it)
    pj_free_tree(*it);
}

std::vector<pj_term_t *>
pj_find_jit_candidates(pTHX_ OP *o, OP *parentop, OPTreeJITCandidateFinder *visitor);

/* Walk OP tree recursively, build ASTs, build subtrees */
STATIC pj_term_t *
pj_build_ast(pTHX_ OP *o,
             vector<OPWithImposedType> &subtrees,
             unsigned int *nvariables, OPTreeJITCandidateFinder *visitor)
{
  const unsigned int parent_otype = o->op_type;
  pj_term_t *retval = NULL;
  OP *kid;

  std::vector<pj_term_t *> kid_terms;

  PJ_DEBUG_2("pj_build_ast running on %s. Have %i subtrees right now.\n", OP_NAME(o), (int)subtrees.size());

  unsigned int ikid = 0;
  if (o && (o->op_flags & OPf_KIDS)) {
    for (kid = ((UNOP*)o)->op_first; kid; kid = kid->op_sibling) {
      PJ_DEBUG_2("pj_build_ast considering kid (%u) type %s\n", ikid, OP_NAME(kid));

      const unsigned int otype = kid->op_type;
      if (otype == OP_CONST) {
        PJ_DEBUG("CONST being inlined.\n");
        /* FIXME OP_CONST can also be an int or a string and who-knows-what-else */
        kid_terms.push_back(pj_make_const_dbl(kid, SvNV(cSVOPx_sv(kid)))); /* FIXME replace type by inferred type */
      }
      else if (otype == OP_PADSV) {
        kid_terms.push_back( pj_make_variable(kid, (*nvariables)++, pj_double_type) ); /* FIXME replace pj_double_type with type that's imposed by the current OP */
      }
      else if (otype == OP_NULL) {
        /* compiled out -- FIXME most certainly not correct, in particular for incoming op_next */
        assert(kid->op_flags & OPf_KIDS);
        /* FIXME Only looking at first kid -- is that a limitation on OP_NULL or can other OP classes be NULLed as well? */
        kid_terms.push_back( pj_build_ast(aTHX_ ((UNOP*)kid)->op_first, subtrees, nvariables, visitor) );
      }
      else if (IS_JITTABLE_OP_TYPE(otype)) {
          kid_terms.push_back( pj_build_ast(aTHX_ kid, subtrees, nvariables, visitor) );
        if (kid_terms.back() == NULL) {
          // Failed to build sub-AST
          pj_free_terms_vector(kid_terms);
          return NULL;
        }
      }
      else {
        /* Can't represent OP with AST. So instead,
         * recursively scan for separate candidates and
         * treat as subtree. */
        PJ_DEBUG_1("Cannot represent this OP with AST. Emitting OP tree term in AST. (%s)", OP_NAME(kid));
        pj_find_jit_candidates(aTHX_ kid, o, visitor); /* o is parent of kid */
        kid_terms.push_back( pj_make_optree(kid));

        // FIXME replace pj_double_type with type that's imposed by the current OP
        subtrees.push_back( OPWithImposedType(kid, pj_double_type) );
      }

      ++ikid;
    } /* end for kids */

    /* TODO modulo may have (very?) different behaviour in Perl than in C (or libjit or the platform...) */
#define EMIT_UNOP_CODE(perl_op_type, pj_op_type)            \
    case perl_op_type:                                      \
      assert(ikid == 1);                                    \
      retval = pj_make_unop( o, pj_op_type, kid_terms[0] ); \
      break;
#define EMIT_BINOP_CODE(perl_op_type, pj_op_type)                           \
    case perl_op_type:                                                      \
      assert(ikid == 2);                                                    \
      retval = pj_make_binop( o, pj_op_type, kid_terms[0], kid_terms[1] );  \
      break;
#define EMIT_LISTOP_CODE(perl_op_type, pj_op_type)    \
    case perl_op_type: {                              \
      assert(ikid > 0);                               \
      std::vector<pj_term_t *> kids;                  \
      for (unsigned int i = 0; i < ikid-1; ++i)       \
        kids.push_back(kid_terms[i]);                 \
      retval = pj_make_listop( o, pj_op_type, kids ); \
      break;                                          \
    }

    switch (parent_otype) {
      EMIT_BINOP_CODE(OP_ADD, pj_binop_add)
      EMIT_BINOP_CODE(OP_SUBTRACT, pj_binop_subtract)
      EMIT_BINOP_CODE(OP_MULTIPLY, pj_binop_multiply)
      EMIT_BINOP_CODE(OP_DIVIDE, pj_binop_divide)
      EMIT_BINOP_CODE(OP_POW, pj_binop_pow)
      EMIT_BINOP_CODE(OP_LEFT_SHIFT, pj_binop_left_shift)
      EMIT_BINOP_CODE(OP_RIGHT_SHIFT, pj_binop_right_shift)
      EMIT_BINOP_CODE(OP_EQ, pj_binop_eq)
      EMIT_BINOP_CODE(OP_AND, pj_binop_bool_and)
      EMIT_BINOP_CODE(OP_OR, pj_binop_bool_or)
      EMIT_UNOP_CODE(OP_SIN, pj_unop_sin)
      EMIT_UNOP_CODE(OP_COS, pj_unop_cos)
      EMIT_UNOP_CODE(OP_SQRT, pj_unop_sqrt)
      EMIT_UNOP_CODE(OP_LOG, pj_unop_log)
      EMIT_UNOP_CODE(OP_EXP, pj_unop_exp)
      EMIT_UNOP_CODE(OP_INT, pj_unop_perl_int)
      EMIT_UNOP_CODE(OP_NOT, pj_unop_bool_not)
      EMIT_UNOP_CODE(OP_NEGATE, pj_unop_negate)
      /* EMIT_UNOP_CODE(OP_COMPLEMENT, pj_unop_bitwise_not) */ /* FIXME not same as perl */
      EMIT_LISTOP_CODE(OP_COND_EXPR, pj_listop_ternary)
    default:
      PJ_DEBUG_1("Shouldn't happen! Unsupported OP!? %s", OP_NAME(o));
      abort();
    }
#undef EMIT_BINOP_CODE
#undef EMIT_UNOP_CODE
#undef EMIT_LISTOP_CODE

  } /* end if has kids */
  else { // OP without kids
    // OP_PADSV and OP_CONST are handled in the caller as other OPs' kids
    PJ_DEBUG_1("ARG! Unsupported OP without kids, %s", OP_NAME(o));
    abort();
  }

  /* PMOP doesn't matter for JIT right now */
  /*
    if (o && OP_CLASS(o) == OA_PMOP && o->op_type != OP_PUSHRE
          && (kid = PMOP_pmreplroot(cPMOPo)))
    {}
  */

  PJ_DEBUG_1("Returning from pj_build_ast. Have %i subtrees right now.\n", (int)subtrees.size());
  return retval;
}

/* Builds op_sibling list between JITOP children, but also
 * re-wires the direct children's op_next to the following
 * child's first OP and the last child's op_next to the JITOP itself. */
static void
pj_build_jitop_kid_list(pTHX_ LISTOP *jitop, vector<OPWithImposedType> &subtrees)
{
  if (subtrees.empty()) {
    jitop->op_first = NULL; /* FIXME is this valid for a LISTOP? */
    jitop->op_last = NULL;
  }
  else {
    OP *o;

    /* TODO for now, we just always impose "numeric". Later, this may need
     *      to be flexible. */

    o = (OP *)subtrees[0].op;
    jitop->op_first = o;
    PJ_DEBUG_1("First kid is %s\n", OP_NAME(o));

    /* Alternating op-imposed-type and actual subtree */
    const unsigned int n = subtrees.size();
    for (unsigned int i = 1; i < n; i += 1) {
      PJ_DEBUG_2("Kid %u is %s\n", i, OP_NAME(o));
      /* TODO get the imposed type context from subtrees[i].imposed_type here */
      o->op_sibling = (OP *)subtrees[i].op;
      o->op_next = pj_find_first_executed_op(aTHX_ subtrees[i].op);
      o = o->op_sibling;
    }

    /* Wire last child OP to execute JITOP next. */
    jitop->op_last = o;
    o->op_next = (OP *)jitop;
    o->op_sibling = NULL;
  }
}


static void
pj_fixup_parent_op(pTHX_ OP *jitop, OP *origop, UNOP *parentop)
{
  OP *kid;

  PJ_DEBUG_1("Doing parent fixups for %s\n", OP_NAME((OP *)parentop));

  /* FIXME the real question is why parent OP is ENTER? */
  /*while (!(parentop->op_flags & OPf_KIDS)) {
    parentop = parentop->op_sibling;
  }
  PJ_DEBUG_1("Doing parent fixups for %s\n", OP_NAME((OP *)parentop));
  */
  jitop->op_next = origop->op_next;

  if (parentop->op_first == origop) {
    parentop->op_first = jitop;
    jitop->op_sibling = origop->op_sibling;
    if (jitop->op_sibling) {
      jitop->op_next = pj_find_first_executed_op(aTHX_ jitop->op_sibling);
    }
    /*
    else {
      jitop->op_next = (OP *)parentop;
    }
    */
  }
  else {
    for (kid = parentop->op_first; kid; kid = kid->op_sibling) {
      PJ_DEBUG_1("%p\n", kid);
      PJ_DEBUG_1("%s\n", OP_NAME(kid));
      if (kid->op_sibling && kid->op_sibling == origop) {
        kid->op_sibling = jitop;
        jitop->op_sibling = origop->op_sibling;
        /* wire JITOP's op_next to the actual next OP */
        if (jitop->op_sibling) {
          jitop->op_next = pj_find_first_executed_op(aTHX_ jitop->op_sibling);
        }
        /*
        else {
          jitop->op_next = (OP *)parentop;
        }
        */
        break;
      }
    }
  }

  /* Fixup op_last of parent op */
  if (OP_CLASS((OP *)parentop) != OA_UNOP) {
    if (((BINOP *)parentop)->op_last == origop)
      ((BINOP *)parentop)->op_last = jitop;
  }

  jitop->op_sibling = origop->op_sibling;
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

static pj_term_t *
pj_attempt_jit(pTHX_ OP *o, OP *parentop, OPTreeJITCandidateFinder *visitor)
{
  pj_term_t *ast;
  unsigned int nvariables = 0;

  if (PJ_DEBUGGING)
    printf("Attempting JIT on %s (%p, %p)\n", OP_NAME(o), o, o->op_next);

  std::vector<OPWithImposedType> subtrees;
  ast = pj_build_ast(aTHX_ o, subtrees, &nvariables, visitor);

  return ast;
}

namespace PerlJIT {
  class OPTreeJITCandidateFinder : public OPTreeVisitor
  {
  public:
    OPTreeJITCandidateFinder() {}

    visit_control_t
    visit_op(pTHX_ OP *o, OP *parentop)
    {
      unsigned int otype;
      otype = o->op_type;

      PJ_DEBUG_1("Considering %s\n", OP_NAME(o));

      /* Attempt JIT if the right OP type. Don't recurse if so. */
      if (IS_JITTABLE_ROOT_OP_TYPE(otype)) {
          pj_term_t *ast = pj_attempt_jit(aTHX_ o, parentop, this);
        if (ast)
            candidates.push_back(ast);
        return VISIT_SKIP;
      }
      return VISIT_CONT;
    } // end 'visit_op'

    std::vector<pj_term_t *> candidates;
  }; // end class OPTreeJITCandidateFinder
}

/* Traverse OP tree from o until done OR a candidate for JITing was found.
 * For candidates, invoke JIT attempt and then move on without going into
 * the particular sub-tree; tree walking in OPTreeWalker, actual logic in
 * OPTreeJITCandidateFinder! */
std::vector<pj_term_t *>
pj_find_jit_candidates(pTHX_ OP *o, OP *parentop, OPTreeJITCandidateFinder *visitor)
{
  visitor->visit(aTHX_ o, parentop);
  return visitor->candidates;
}

std::vector<pj_term_t *>
pj_find_jit_candidates(pTHX_ OP *o, OP *parentop)
{
  OPTreeJITCandidateFinder f;
  return pj_find_jit_candidates(aTHX_ o, parentop, &f);
}

std::vector<pj_term_t *>
pj_find_jit_candidates(pTHX_ SV *coderef)
{
  if (!SvROK(coderef) || SvTYPE(SvRV(coderef)) != SVt_PVCV)
    croak("Need a code reference");
  CV *cv = (CV *) SvRV(coderef);

  return pj_find_jit_candidates(aTHX_ CvROOT(cv), 0);
}
