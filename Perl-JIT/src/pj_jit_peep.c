#include "pj_jit_peep.h"
#include <stdlib.h>
#include <stdio.h>

#include "stack.h"
#include "pj_debug.h"
#include "pj_global_state.h"
#include "pj_optree.h"

void
pj_jit_peep(pTHX_ OP *o)
{
  pj_find_jit_candidate(aTHX_ o);

  /* May be called one layer deep into the tree, it seems, so respect siblings. */
  while (o->op_sibling) {
    o = o->op_sibling;
    pj_find_jit_candidate(aTHX_ o);
  }

  PJ_orig_peepp(aTHX_ o);
}

