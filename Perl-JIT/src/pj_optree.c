#include "pj_optree.h"
#include <stdlib.h>
#include <stdio.h>

#include "ppport.h"

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
static void
pj_attempt_jit(pTHX_ OP *o)
{
}


/* inspired by B.xs */
#define PMOP_pmreplstart(o)	o->op_pmstashstartu.op_pmreplstart
#define PMOP_pmreplroot(o)	o->op_pmreplrootu.op_pmreplroot
/* Traverse OP tree from o until done OR a candidate for JITing was found.
 * For candidates, invoke JIT attempt and then move on without going into
 * the particular sub-tree. */
void
pj_find_jit_candidate(pTHX_ OP *o)
{
  /* FIXME: limit depth of recursion of write iterative version with
   *        stack instead. pj_find_jit_candidate should be relatively easy
   *        to do with a stack (see stack.h in same directory!). */

  OP *kid;

  if (o && (o->op_flags & OPf_KIDS)) {
    for (kid = ((UNOP*)o)->op_first; kid; kid = kid->op_sibling) {
      pj_find_jit_candidate(aTHX_ kid);
    }
  }

  if (o && OP_CLASS(o) == OA_PMOP && o->op_type != OP_PUSHRE
        && (kid = PMOP_pmreplroot(cPMOPo)))
  {
    pj_find_jit_candidate(aTHX_ kid);
  }
}

