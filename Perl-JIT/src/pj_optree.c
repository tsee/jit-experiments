#include "pj_optree.h"
#include <stdlib.h>
#include <stdio.h>

#include "ppport.h"
#include "pj_debug.h"
#include "stack.h"

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
  PJ_DEBUG_1("Attempting JIT on %s\n", OP_NAME(o));
}

#define IS_JITTABLE_OP_TYPE(otype) (otype == OP_ADD)

/* inspired by B.xs */
#define PMOP_pmreplstart(o)	o->op_pmstashstartu.op_pmreplstart
#define PMOP_pmreplroot(o)	o->op_pmreplrootu.op_pmreplroot

/* Traverse OP tree from o until done OR a candidate for JITing was found.
 * For candidates, invoke JIT attempt and then move on without going into
 * the particular sub-tree. */
void
pj_find_jit_candidate(pTHX_ OP *o)
{
  const unsigned int otype = o->op_type;
  OP *kid;
  ptrstack_t *backlog;

  backlog = ptrstack_make(5, 0);
  ptrstack_push(backlog, o);

  /* Iterative tree traversal using stack */
  while (!ptrstack_empty(backlog)) {
    o = ptrstack_pop(backlog);

    PJ_DEBUG_1("Considering %s\n", OP_NAME(o));

    if (IS_JITTABLE_OP_TYPE(otype))
      pj_attempt_jit(aTHX_ o);

    if (o && (o->op_flags & OPf_KIDS)) {
      for (kid = ((UNOP*)o)->op_first; kid; kid = kid->op_sibling) {
        ptrstack_push(backlog, kid);
      }
    }

    if (o && OP_CLASS(o) == OA_PMOP && o->op_type != OP_PUSHRE
          && (kid = PMOP_pmreplroot(cPMOPo)))
    {
      ptrstack_push(backlog, kid);
    }
  }

  ptrstack_free(backlog);
}

