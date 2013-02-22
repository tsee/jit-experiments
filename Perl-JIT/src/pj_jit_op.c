#include "pj_jit_op.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "pj_debug.h"
#include "pj_global_state.h"
#include "pj_jit.h"

OP *
pj_pp_jit(pTHX)
{
  dVAR; dSP;
  /* Traditionally, addop uses dATARGET here, but that relies
   * on being able to use PL_op->op_targ as a PAD offset sometimes.
   * For the JIT OP, this info comes from the aux struct, so we need
   * to inline a modified version of dATARGET. */
  SV *targ;

  pj_jitop_aux_t *aux = (pj_jitop_aux_t *) ((BINOP *)PL_op)->op_targ;

  SV *tmpsv;
  unsigned int i, n;

  //printf("Custom op called\n");

  /* inlined modified dATARGET, see above */
  targ = PL_op->op_flags & OPf_STACKED
         ? sp[-1]
         : PAD_SV(aux->saved_op_targ);

  /* This implements overload and other magic horribleness */
  tryAMAGICbin_MG(add_amg, AMGf_assign|AMGf_numeric);

  {
    double result; /* FIXME function ret type should be dynamic */
    double *params = aux->paramslist;
    n = aux->nparams;

    /* Pop all args from stack except the last */
    for (i = 0; i < n-1; ++i) {
      tmpsv = POPs;
      params[i] = SvNV_nomg(tmpsv);
    }
    /* If there's any args, leave the final one's SV on the stack */
    if (n != 0) {
      tmpsv = TOPs;
      params[n-1] = SvNV_nomg(tmpsv);
    }
    //printf("In: %f %f\n", params[0], params[1]);
    pj_invoke_func((pj_invoke_func_t) aux->jit_fun, params, aux->nparams, pj_double_type, (void *)&result);

    PJ_DEBUG_1("Add result from JIT: %f\n", (float)result);
    SETn((NV)result);
  }

  RETURN;
}


/* Hook that will free the JIT OP aux structure of our custom ops */
/* FIXME this doesn't appear to actually be called for all ops -
 *       specifically NOT for our custom OP. Is this because the
 *       custom OP isn't wired up correctly? */
void
pj_jitop_free_hook(pTHX_ OP *o)
{
  if (PJ_orig_opfreehook != NULL)
    PJ_orig_opfreehook(aTHX_ o);

  /* printf("cleaning %s\n", OP_NAME(o)); */
  if (o->op_ppaddr == pj_pp_jit) {
    PJ_DEBUG("Cleaning up custom OP's pj_jitop_aux_t\n");
    pj_jitop_aux_t *aux = (pj_jitop_aux_t *)o->op_targ;
    free(aux->paramslist);
    free(aux);
    o->op_targ = 0; /* important or Perl will use it to access the pad */
  }
}
