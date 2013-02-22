#ifndef PJ_JIT_OP_H_
#define PJ_JIT_OP_H_

/* Code related to the run-time implementation of the actual
 * JIT OP. */

#include <EXTERN.h>
#include <perl.h>

/* The struct of pertinent per-OP instance
 * data that we attach to each JIT OP. */
typedef struct {
  void (*jit_fun)(void);
  NV *paramslist;
  UV nparams;
  PADOFFSET saved_op_targ; /* Replacement for JIT OP's op_targ if necessary */
} pj_jitop_aux_t;

/* The generic custom OP implementation - push/pop function */
OP *pj_pp_jit(pTHX);

/* Hook that will free the JIT OP aux structure of our custom ops */
void pj_jitop_free_hook(pTHX_ OP *o);

#endif
