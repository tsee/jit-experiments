#include "pj_jit_peep.h"
#include <stdlib.h>
#include <stdio.h>

#include "stack.h"
#include "pj_debug.h"
#include "pj_global_state.h"

void
pj_jit_peep(pTHX_ OP *o)
{
  pj_find_jit_candidate(aTHX_ o);

  /* may be called one layer deep into the tree, it seems, so respect siblings. */
  while (o->op_sibling) {
    o = o->op_sibling;
    pj_find_jit_candidate(aTHX_ o);
  }

  PJ_orig_peepp(aTHX_ o);

  return; /* Do not run old JIT code any more: Being reworked! */


  OP *root = o;

  PJ_DEBUG_2("Looking at: %s (%s)\n", OP_NAME(o), OP_DESC(o));
  /* Currently disabled lexicalization hacks/experiments */
  /*
  hint = cop_hints_fetch_pvs(PL_curcop, "jit", 0);
  if (hint != NULL && SvTRUE(hint)) {
    printf("Optimizing this op!\n");
  }
  else {
    printf("NOT optimizing this op!\n");
  }
  */

  ptrstack_t *tovisit;
  tovisit = ptrstack_make(20, 0);

  OP *kid = o;
  while (kid->op_sibling != NULL) {
    kid = kid->op_sibling;
    ptrstack_push(tovisit, root);
    ptrstack_push(tovisit, kid);
  }

  OP *parent = root;
  while (!ptrstack_empty(tovisit)) {
    int do_recurse = 1;
    o = ptrstack_pop(tovisit);
    parent = ptrstack_pop(tovisit);

    if (PJ_DEBUGGING)
      printf("Walking: Looking at: %s (%s); parent: %s\n", OP_NAME(o), OP_DESC(o), OP_NAME(parent));

    if (o->op_type == OP_ADD)
    {
      if (   (cBINOPo->op_first->op_type == OP_PADSV || cBINOPo->op_first->op_type == OP_CONST)
          && (cBINOPo->op_last->op_type == OP_PADSV || cBINOPo->op_last->op_type == OP_CONST) )
      {
        PJ_DEBUG_1("Found candidate with parent of type %s!\n", OP_NAME(parent));
        attempt_add_jit_proof_of_principle(aTHX_ cBINOPo, parent);
        do_recurse = 0; /* for now */
      }
    }

    /* TODO s/// can have more stuff inside but not OPf_KIDS set? (can steal from B::Concise later) */
    if (do_recurse && o->op_flags & OPf_KIDS) {
      PJ_DEBUG_1("Who knew? %s has kids.\n", OP_NAME(o));
      for (kid = cBINOPo->op_first; kid != NULL; kid = kid->op_sibling) {
        ptrstack_push(tovisit, o);
        ptrstack_push(tovisit, kid);
      }
    }
  }

  ptrstack_free(tovisit);
}

