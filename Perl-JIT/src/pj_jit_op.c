#include "pj_jit_op.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "pj_debug.h"
#include "pj_global_state.h"
#include "pj_ast_jit.h"

OP *
pj_pp_jit(pTHX)
{
  dVAR; dSP;
  /* Traditionally, addop uses dATARGET here, but that relies
   * on being able to use PL_op->op_targ as a PAD offset sometimes.
   * For the JIT OP, this info comes from the aux struct, so we need
   * to inline a modified version of dATARGET. */
  dTARG;

  pj_jitop_aux_t *aux = (pj_jitop_aux_t *) ((BINOP *)PL_op)->op_targ;

  SV *tmpsv;
  unsigned int i, n;

  PJ_DEBUG_1("Custom op '%s' called\n", OP_NAME(PL_op));

  /* inlined modified dATARGET, see above */
  TARG = PL_op->op_private & OPf_STACKED
         ? sp[-1]
         : PAD_SV(aux->saved_op_targ);
  if (PJ_DEBUGGING && PL_op->op_private & OPf_STACKED) {
    PJ_DEBUG("Params arrived on stack -- using sp[-1] as TARG\n");
    sv_dump(TARG);
  }
  else if (PJ_DEBUGGING) {
    PJ_DEBUG_1("Using PAD_SV(%i) as TARG\n", (int)aux->saved_op_targ);
    sv_dump(TARG);
  }

  /* This implements overload and other magic horribleness */
  /* FIXME What should this do for a generic JIT OP replacing a subtree?
   *       Probably, the answer is "not supported". */
  /* tryAMAGICbin_MG(add_amg, AMGf_assign|AMGf_numeric); */

  {
    double result; /* FIXME function ret type should be dynamic */
    double *params = aux->paramslist;
    n = aux->nparams;

    PJ_DEBUG_1("Expecting %u parameters on stack.\n", n);
    /* FIXME future optimization: Don't pop the last param off the stack but reuse. */
    /* Pop all args from stack */
    for (i = 0; i < n-1; ++i) {
      tmpsv = POPs;
      params[n-i-1] = SvNV_nomg(tmpsv);
      PJ_DEBUG_2("Param %i is %f.\n", n-i-1, params[n-i-1]);
    }
    if (n != 0) {
      tmpsv = TOPs;
      params[0] = SvNV_nomg(tmpsv);
      PJ_DEBUG_2("Param %i is %f.\n", 0, params[0]);
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


LISTOP *
pj_prepare_jit_op(pTHX_ const unsigned int nvariables, OP *origop)
{
  LISTOP *jitop;
  pj_jitop_aux_t *jit_aux;

  NewOp(1101, jitop, 1, LISTOP);
  jitop->op_type = (OPCODE)OP_CUSTOM;
  jitop->op_next = (OP *)jitop;
  /* If OPpTARGET_MY is set on the original OP, then we have a nasty situation.
   * In a nutshell, this is set as an optimization for scalar assignment
   * to a pad (== lexical) variable. If set, the addop will directly
   * assign to whichever pad variable would otherwise be set by the sassign
   * op. It won't bother putting a separate var on the stack.
   * This is great, but it uses the op_targ member of the OP struct to
   * define the offset into the pad where the output variable is to be found.
   * That's a problem because we're using op_targ to hang the jit aux struct
   * off of.
   */
  jitop->op_private = (origop->op_private & OPpTARGET_MY ? OPpTARGET_MY : 0);
  jitop->op_flags = (nvariables > 0 ? (OPf_STACKED|OPf_KIDS) : 0);

  /* Set it's implementation ptr */
  jitop->op_ppaddr = pj_pp_jit;

  /* Init jit_aux */
  jit_aux = malloc(sizeof(pj_jitop_aux_t));
  jit_aux->paramslist = (NV *)malloc(sizeof(NV) * nvariables);
  jit_aux->nparams = nvariables;
  jit_aux->jit_fun = NULL;
  jit_aux->saved_op_targ = origop->op_targ; /* save in case needed for sassign optimization */
  /* FIXME is copying op_targ good enough? */

  /* It may turn out that op_targ is not safe to use for custom OPs because
   * some core functions may meddle with it. But chances are it's fine.
   * If not, we'll need to become extra-creative... */
  jitop->op_targ = (PADOFFSET)PTR2UV(jit_aux);

  return jitop;
}
