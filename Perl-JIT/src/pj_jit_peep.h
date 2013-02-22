#ifndef PJ_JIT_PEEP_H_
#define PJ_JIT_PEEP_H_

/* Code related to running the actual peephole optimizer phase that
 * finds JITable subtrees. */

#include <EXTERN.h>
#include <perl.h>

/* The custom peephole optimizer-wrapper */
void pj_jit_peep(pTHX_ OP *o);

#endif
