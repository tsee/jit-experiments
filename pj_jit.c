
#include "pj_jit.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <pj_walkers.h>

static jit_value_t pj_jit_internal_op(jit_function_t function, jit_value_t *var_values, int nvars, pj_op_t *op);

static jit_value_t
pj_jit_internal(jit_function_t function, jit_value_t *var_values, int nvars, pj_term_t *term)
{
  if (term->type == pj_ttype_variable) {
    pj_variable_t *v = (pj_variable_t *)term;
    return var_values[v->ivar];
  }
  else if (term->type == pj_ttype_constant) {
    pj_constant_t *c = (pj_constant_t *)term;
    if (c->const_type == pj_int_type)
      return jit_value_create_nint_constant(function, jit_type_sys_int, c->value_u.int_value);
    else if (c->const_type == pj_uint_type) /* FIXME no jit_value_create_nuint_constant defined? */
      return jit_value_create_nint_constant(function, jit_type_sys_int, c->value_u.uint_value);
    else if (c->const_type == pj_double_type)
      return jit_value_create_float64_constant(function, jit_type_sys_double, c->value_u.dbl_value);
    else
      abort();
  }
  else if (term->type == pj_ttype_op) {
    return pj_jit_internal_op(function, var_values, nvars, (pj_op_t *)term);
  }
  else {
    abort();
  }
}

static jit_value_t
pj_jit_internal_op(jit_function_t function, jit_value_t *var_values, int nvars, pj_op_t *op)
{
  jit_value_t tmp1, tmp2, rv;
  tmp1 = pj_jit_internal(function, var_values, nvars, op->op1);
  if (op->op2 != NULL)
    tmp2 = pj_jit_internal(function, var_values, nvars, op->op2);

  switch (op->optype) {
  case pj_unop_negate:
    rv = jit_insn_neg(function, tmp1);
    break;
  case pj_unop_sin:
    rv = jit_insn_sin(function, tmp1);
    break;
  case pj_unop_cos:
    rv = jit_insn_cos(function, tmp1);
    break;
  case pj_unop_abs:
    rv = jit_insn_abs(function, tmp1);
    break;
  case pj_unop_sqrt:
    rv = jit_insn_sqrt(function, tmp1);
    break;
  case pj_unop_log:
    rv = jit_insn_log(function, tmp1);
    break;
  case pj_unop_exp:
    rv = jit_insn_exp(function, tmp1);
    break;
  case pj_binop_add:
    rv = jit_insn_add(function, tmp1, tmp2);
    break;
  case pj_binop_subtract:
    rv = jit_insn_sub(function, tmp1, tmp2);
    break;
  case pj_binop_multiply:
    rv = jit_insn_mul(function, tmp1, tmp2);
    break;
  case pj_binop_divide:
    rv = jit_insn_div(function, tmp1, tmp2);
    break;
  case pj_binop_modulo:
    rv = jit_insn_rem(function, tmp1, tmp2); /* FIXME should this use jit_insn_rem_ieee? */
    break;
  case pj_binop_atan2:
    rv = jit_insn_atan2(function, tmp1, tmp2);
    break;
  default:
    abort();
  }

  return rv;
}


int
pj_tree_jit(jit_context_t context, pj_term_t *term, jit_function_t *outfun, pj_basic_type *funtype)
{
  unsigned int i;
  jit_function_t function;
  jit_type_t *params;
  jit_type_t signature;

  jit_context_build_start(context);

  /* Get the "function type" which is the type that will be used for the
   * return value as well as all arguments. Double trumps ints. */
  *funtype = pj_tree_determine_funtype(term);

  /* Extract all variable occurrances from the AST */
  pj_variable_t **vars;
  unsigned int nvars;
  pj_tree_extract_vars(term, &vars, &nvars);
  printf("Found %i variable occurrances in tree.\n", nvars);

  /* Naive assumption: the maximum ivar is the total number if distinct arguments (-1) */
  unsigned int max_var = 0;
  for (i = 0; i < nvars; ++i) {
    if (max_var < vars[i]->ivar)
      max_var = vars[i]->ivar;
  }
  printf("Found %i distinct variables in tree.\n", 1+max_var);
  nvars = max_var+1;
  free(vars);

  /* Setup libjit func signature */
  params = (jit_type_t *)malloc(nvars*sizeof(jit_type_t));
  for (i = 0; (int)i < nvars; ++i) {
    params[i] = (*funtype == pj_int_type ? jit_type_sys_int : jit_type_sys_double);
  }
  signature = jit_type_create_signature(
    jit_abi_cdecl,
    (*funtype == pj_int_type ? jit_type_sys_int : jit_type_sys_double),
    params,
    nvars,
    1
  );
  function = jit_function_create(context, signature);

  /* Setup libjit values for func params */
  jit_value_t *var_values;
  var_values = (jit_value_t *)malloc(nvars*sizeof(jit_value_t));
  for (i = 0; (int)i < nvars; ++i) {
    var_values[i] = jit_value_get_param(function, i);
  }

  /* Recursively emit instructions for JIT and final return */
  jit_value_t rv = pj_jit_internal(function, var_values, nvars, term);
  jit_insn_return(function, rv);

  /* Make it so! */
  /* jit_function_set_optimization_level(function, jit_function_get_max_optimization_level()); */
  jit_function_compile(function);
  jit_context_build_end(context);

  *outfun = function;
  return 0;
}

/* Aaaaaaarg! */
#include "pj_type_switch.h"

void
pj_invoke_func(pj_invoke_func_t fptr, void *args, unsigned int nargs, pj_basic_type funtype, void *retval)
{
  assert(nargs <= 20);
  if (funtype == pj_double_type) {
    PJ_TYPE_SWITCH(double, args, nargs, retval);
  }
  else if (funtype == pj_int_type) {
    PJ_TYPE_SWITCH(int, args, nargs, retval)
  }
  else
    abort();
}
#undef PJ_TYPE_SWITCH

