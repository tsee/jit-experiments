#include "pj_global_state.h"
#include "pj_inline.h"
#include "pj_debug.h"
#include "pj_ast_terms.h"
#include "pj_keyword_plugin.h"

//XOP PJ_xop_jitop;
//Perl_ophook_t PJ_orig_opfreehook;
//jit_context_t PJ_jit_context = NULL; /* jit_context_t is a ptr */

/* For chaining keyword plugins */
int (*PJ_next_keyword_plugin)(pTHX_ char *, STRLEN, OP **);

/* TODO: Make jit_context_t interpreter-local */
void
pj_init_global_state(pTHX)
{
  // Setup the actual keyword plugin
  PJ_next_keyword_plugin = PL_keyword_plugin;
  PL_keyword_plugin = pj_jit_type_keyword_plugin;

  /* Setup our callback for cleaning up JIT OPs during global cleanup */
  //PJ_orig_opfreehook = PL_opfreehook;
  //PL_opfreehook = pj_jitop_free_hook;

  /* Setup our custom op */
  //XopENTRY_set(&PJ_xop_jitop, xop_name, "jitop");
  //XopENTRY_set(&PJ_xop_jitop, xop_desc, "a just-in-time compiled composite operation");
  //XopENTRY_set(&PJ_xop_jitop, xop_class, OA_LISTOP);
  //Perl_custom_op_register(aTHX_ pj_pp_jit, &PJ_xop_jitop);
}

#define INT_CONST(name) \
  pj_define_int(aTHX_ name, #name)

#define STRING_CONST(name) \
  pj_define_string(aTHX_ name, strlen(name), #name)

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

static void pj_define_string(pTHX_ const char *value, const STRLEN value_len, const char *name)
{
  char buffer[64];

  strcpy(buffer, "Perl::JIT::");
  strcat(buffer, name);

  SV *sv = get_sv(buffer, GV_ADD);
  HV *hv = gv_stashpvs("Perl::JIT", GV_ADD);

  sv_setpvn(sv, value, value_len);
  newCONSTSUB(hv, name, sv);

  AV *export_ok = get_av("Perl::JIT::EXPORT_OK", GV_ADD);
  av_push(export_ok, newSVpv(name, 0));
}

void pj_define_constants(pTHX)
{
  INT_CONST(pj_ttype_constant);
  INT_CONST(pj_ttype_lexical);
  INT_CONST(pj_ttype_global);
  INT_CONST(pj_ttype_variabledeclaration);
  INT_CONST(pj_ttype_list);
  INT_CONST(pj_ttype_optree);
  INT_CONST(pj_ttype_nulloptree);
  INT_CONST(pj_ttype_op);
  INT_CONST(pj_ttype_statementsequence);
  INT_CONST(pj_ttype_statement);

  INT_CONST(pj_sigil_scalar);
  INT_CONST(pj_sigil_array);
  INT_CONST(pj_sigil_hash);
  INT_CONST(pj_sigil_glob);

  INT_CONST(pj_opc_unop);
  INT_CONST(pj_opc_binop);
  INT_CONST(pj_opc_listop);
  INT_CONST(pj_opc_block);

  INT_CONST(pj_context_caller);
  INT_CONST(pj_context_void);
  INT_CONST(pj_context_scalar);
  INT_CONST(pj_context_list);

#include "pj_ast_ops_const-gen.inc"

  INT_CONST(pj_unop_FIRST);
  INT_CONST(pj_unop_LAST);

  INT_CONST(pj_binop_FIRST);
  INT_CONST(pj_binop_LAST);

  INT_CONST(pj_listop_FIRST);
  INT_CONST(pj_listop_LAST);

  INT_CONST(pj_unspecified_type);
  INT_CONST(pj_any_type);
  INT_CONST(pj_scalar_type);
  INT_CONST(pj_gv_type);
  INT_CONST(pj_opaque_type);
  INT_CONST(pj_string_type);
  INT_CONST(pj_double_type);
  INT_CONST(pj_int_type);
  INT_CONST(pj_uint_type);

  STRING_CONST(PJ_KEYWORD_PLUGIN_HINT);
}
