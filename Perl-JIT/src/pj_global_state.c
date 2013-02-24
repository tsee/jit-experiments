#include "pj_global_state.h"
#include "pj_debug.h"
#include "pj_jit_peep.h"
#include "pj_jit_op.h"

XOP PJ_xop_jitop;
peep_t PJ_orig_peepp;
Perl_ophook_t PJ_orig_opfreehook;
jit_context_t PJ_jit_context = NULL; /* jit_context_t is a ptr */

/* TODO: Make jit_context_t interpreter-local */
void
pj_init_global_state(pTHX)
{
  /* Setup our new peephole optimizer */
  PJ_orig_peepp = PL_peepp;
  PL_peepp = pj_jit_peep;

  /* Set up JIT compiler */
  PJ_jit_context = jit_context_create();

  /* Setup our callback for cleaning up JIT OPs during global cleanup */
  PJ_orig_opfreehook = PL_opfreehook;
  PL_opfreehook = pj_jitop_free_hook;

  /* Setup our custom op */
  XopENTRY_set(&PJ_xop_jitop, xop_name, "jitop");
  XopENTRY_set(&PJ_xop_jitop, xop_desc, "a just-in-time compiled composite operation");
  XopENTRY_set(&PJ_xop_jitop, xop_class, OA_LISTOP);
  Perl_custom_op_register(aTHX_ pj_pp_jit, &PJ_xop_jitop);

  /* Register super-late global cleanup hook for global JIT state */
  Perl_call_atexit(aTHX_ pj_global_state_final_cleanup, NULL);
}

/* End-of-global-destruction cleanup hook.
 * Actually installed in BOOT XS section. */
void
pj_global_state_final_cleanup(pTHX_ void *ptr)
{
  (void)ptr;
  
  PJ_DEBUG("pj_jit_final_cleanup after global destruction.\n");

  if (PJ_jit_context != NULL)
    jit_context_destroy(PJ_jit_context);
}
