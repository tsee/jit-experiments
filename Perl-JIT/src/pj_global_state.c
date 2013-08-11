#include "pj_global_state.h"
#include "pj_debug.h"
#include "pj_jit_op.h"

XOP PJ_xop_jitop;
Perl_ophook_t PJ_orig_opfreehook;
jit_context_t PJ_jit_context = NULL; /* jit_context_t is a ptr */

/* TODO: Make jit_context_t interpreter-local */
void
pj_init_global_state(pTHX)
{
  /* Setup our callback for cleaning up JIT OPs during global cleanup */
  PJ_orig_opfreehook = PL_opfreehook;
  PL_opfreehook = pj_jitop_free_hook;

  /* Setup our custom op */
  XopENTRY_set(&PJ_xop_jitop, xop_name, "jitop");
  XopENTRY_set(&PJ_xop_jitop, xop_desc, "a just-in-time compiled composite operation");
  XopENTRY_set(&PJ_xop_jitop, xop_class, OA_LISTOP);
  Perl_custom_op_register(aTHX_ pj_pp_jit, &PJ_xop_jitop);
}

#define INT_CONST(name) \
  pj_define_int(aTHX_ name, #name)

static void pj_define_int(pTHX_ IV value, const char *name)
{
  char buffer[64];

  strcpy(buffer, "Perl::JIT::");
  strcat(buffer, name);

  SV *sv = get_sv(buffer, GV_ADD);
  HV *hv = gv_stashpvs("Perl::JIT", GV_ADD);

  sv_setiv(sv, value);
  newCONSTSUB(hv, name, sv);

  AV *export_ok = get_av("Perl::JIT::EXPORT_OK", GV_ADD);
  av_push(export_ok, newSVpv(name, 0));
}

void pj_define_constants(pTHX)
{
  INT_CONST(pj_ttype_constant);
  INT_CONST(pj_ttype_variable);
  INT_CONST(pj_ttype_optree);
  INT_CONST(pj_ttype_op);

  INT_CONST(pj_unop_negate);
  INT_CONST(pj_unop_sin);
  INT_CONST(pj_unop_cos);
  INT_CONST(pj_unop_abs);
  INT_CONST(pj_unop_sqrt);
  INT_CONST(pj_unop_log);
  INT_CONST(pj_unop_exp);
  INT_CONST(pj_unop_perl_int);
  INT_CONST(pj_unop_bitwise_not);
  INT_CONST(pj_unop_bool_not);

  INT_CONST(pj_binop_add);
  INT_CONST(pj_binop_subtract);
  INT_CONST(pj_binop_multiply);
  INT_CONST(pj_binop_divide);
  INT_CONST(pj_binop_modulo);
  INT_CONST(pj_binop_atan2);
  INT_CONST(pj_binop_pow);
  INT_CONST(pj_binop_left_shift);
  INT_CONST(pj_binop_right_shift);
  INT_CONST(pj_binop_bitwise_and);
  INT_CONST(pj_binop_bitwise_or);
  INT_CONST(pj_binop_bitwise_xor);
  INT_CONST(pj_binop_eq);
  INT_CONST(pj_binop_ne);
  INT_CONST(pj_binop_lt);
  INT_CONST(pj_binop_le);
  INT_CONST(pj_binop_gt);
  INT_CONST(pj_binop_ge);
  INT_CONST(pj_binop_bool_and);
  INT_CONST(pj_binop_bool_or);

  INT_CONST(pj_listop_ternary);

  INT_CONST(pj_unop_FIRST);
  INT_CONST(pj_unop_LAST);

  INT_CONST(pj_binop_FIRST);
  INT_CONST(pj_binop_LAST);

  INT_CONST(pj_listop_FIRST);
  INT_CONST(pj_listop_LAST);

  INT_CONST(pj_double_type);
  INT_CONST(pj_int_type);
  INT_CONST(pj_uint_type);
}
