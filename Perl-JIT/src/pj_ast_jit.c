
#include "pj_ast_jit.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <jit/jit-dump.h>

#include <pj_debug.h>
#include <pj_ast_terms.h>
#include <pj_ast_walkers.h>

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
  jit_value_t arg1, arg2, rv;

#define EVAL_OPERAND(operand) pj_jit_internal(function, var_values, nvars, operand)
#define EVAL_OPERAND1 EVAL_OPERAND(op->op1)
#define EVAL_OPERAND2 EVAL_OPERAND(op->op2)

  /* Only do the recursion out here if we know that we'll have to emit that code at all. */
  if (!(PJ_OP_FLAGS(op) & PJ_ASTf_CONDITIONAL)) {
    arg1 = EVAL_OPERAND1;
    if (op->op2 != NULL)
      arg2 = EVAL_OPERAND2;
  }

  switch (op->optype) {
  case pj_unop_negate:
    rv = jit_insn_neg(function, arg1);
    break;
  case pj_unop_sin:
    rv = jit_insn_sin(function, arg1);
    break;
  case pj_unop_cos:
    rv = jit_insn_cos(function, arg1);
    break;
  case pj_unop_abs:
    rv = jit_insn_abs(function, arg1);
    break;
  case pj_unop_sqrt:
    rv = jit_insn_sqrt(function, arg1);
    break;
  case pj_unop_log:
    rv = jit_insn_log(function, arg1);
    break;
  case pj_unop_exp:
    rv = jit_insn_exp(function, arg1);
    break;
  case pj_unop_perl_int: {
      jit_label_t endlabel = jit_label_undefined;
      jit_label_t neglabel = jit_label_undefined;
      jit_value_t tmprv, tmpval, constval;

      /* if value < 0.0, then goto neglabel */
      constval = jit_value_create_nfloat_constant(function, jit_type_sys_double, 0.0);
      tmpval = jit_insn_lt(function, arg1, constval);
      jit_insn_branch_if(function, tmpval, &neglabel);

      /* else use floor, then goto endlabel */
      rv = jit_insn_floor(function, arg1);
      jit_insn_branch(function, &endlabel);

      /* neglabel: use ceil, fall through to endlabel */
      jit_insn_label(function, &neglabel);
      tmprv = jit_insn_ceil(function, arg1); /* appears to need intermediary? WTF? */
      jit_insn_store(function, rv, tmprv);

      /* endlabel; done. */
      jit_insn_label(function, &endlabel);
      break;
    }
  case pj_unop_bitwise_not: {
      /* FIXME still not same as perl */
      jit_value_t t = jit_insn_convert(function, arg1, jit_type_sys_ulong, 0); /* FIXME replace jit_type_sys_ulong with whatever a UV is */
      rv = jit_insn_not(function, t);
      break;
  }
  case pj_unop_bool_not:
    rv = jit_insn_to_not_bool(function, arg1);
    break;
  case pj_binop_add:
    rv = jit_insn_add(function, arg1, arg2);
    break;
  case pj_binop_subtract:
    rv = jit_insn_sub(function, arg1, arg2);
    break;
  case pj_binop_multiply:
    rv = jit_insn_mul(function, arg1, arg2);
    break;
  case pj_binop_divide:
    rv = jit_insn_div(function, arg1, arg2);
    break;
  case pj_binop_modulo:
    rv = jit_insn_rem(function, arg1, arg2); /* FIXME should this use jit_insn_rem_ieee? */
    break;
  case pj_binop_atan2:
    rv = jit_insn_atan2(function, arg1, arg2);
    break;
  case pj_binop_pow:
    rv = jit_insn_pow(function, arg1, arg2);
    break;
  case pj_binop_left_shift:
    rv = jit_insn_shl(function, arg1, arg2);
    break;
  case pj_binop_right_shift:
    rv = jit_insn_ushr(function, arg1, arg2);
    break;
  case pj_binop_bitwise_and:
    rv = jit_insn_and(function, arg1, arg2);
    break;
  case pj_binop_bitwise_or:
    rv = jit_insn_or(function, arg1, arg2);
    break;
  case pj_binop_bitwise_xor:
    rv = jit_insn_xor(function, arg1, arg2);
    break;
  case pj_binop_eq:
    rv = jit_insn_eq(function, arg1, arg2);
    break;
  case pj_binop_ne:
    rv = jit_insn_ne(function, arg1, arg2);
    break;
  case pj_binop_lt:
    rv = jit_insn_lt(function, arg1, arg2);
    break;
  case pj_binop_le:
    rv = jit_insn_le(function, arg1, arg2);
    break;
  case pj_binop_gt:
    rv = jit_insn_gt(function, arg1, arg2);
    break;
  case pj_binop_ge:
    rv = jit_insn_ge(function, arg1, arg2);
    break;
  case pj_binop_bool_and: {
      jit_label_t endlabel = jit_label_undefined;

      rv = EVAL_OPERAND1;
      /* If value is false, then goto end */
      jit_insn_branch_if_not(function, rv, &endlabel);

      /* Left is true, move to right operand */
      arg2 = EVAL_OPERAND2;
      jit_insn_store(function, rv, arg2);

      /* endlabel; done. */
      jit_insn_label(function, &endlabel);
      break;
    }
  case pj_binop_bool_or: {
      jit_label_t endlabel = jit_label_undefined;

      rv = EVAL_OPERAND1;
      /* If value is true, then goto end */
      jit_insn_branch_if(function, rv, &endlabel);

      /* Left is false, move to right operand */
      arg2 = EVAL_OPERAND2;
      jit_insn_store(function, rv, arg2);

      /* endlabel; done. */
      jit_insn_label(function, &endlabel);
      break;
    }
  case pj_listop_ternary: {
      jit_label_t rightlabel = jit_label_undefined;
      jit_label_t endlabel = jit_label_undefined;
      jit_value_t cond;
      pj_term_t *operand;
      rv = jit_value_create(function, jit_type_sys_double);

      /* operands are linked list of "condition", "true-value (left)", "false-value (right)" */
      operand = op->op1;

      cond = EVAL_OPERAND(operand);
      /* If value is false, then goto right branch */
      jit_insn_branch_if_not(function, cond, &rightlabel);

      /* Left is true, return result of evaluating left operand */
      operand = operand->op_sibling;
      arg1 = EVAL_OPERAND(operand);
      jit_insn_store(function, rv, arg1);
      jit_insn_branch(function, &endlabel);

      /* Left is false, return result of evaluating right operand */
      jit_insn_label(function, &rightlabel);
      operand = operand->op_sibling;
      arg2 = EVAL_OPERAND(operand);
      jit_insn_store(function, rv, arg2);

      /* endlabel; done. */
      jit_insn_label(function, &endlabel);
      printf("\n");
      break;
    }
  default:
    abort();
  }

#undef EVAL_OPERAND1
#undef EVAL_OPERAND2

  /*
  if (PJ_DEBUGGING)
    jit_dump_function(stdout, function, "func");
  */

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
  PJ_DEBUG_1("Found %i variable occurrances in tree.\n", nvars);

  /* Naive assumption: the maximum ivar is the total number if distinct arguments (-1) */
  unsigned int max_var = 0;
  for (i = 0; i < nvars; ++i) {
    if (max_var < (unsigned int)vars[i]->ivar)
      max_var = vars[i]->ivar;
  }
  PJ_DEBUG_1("Found %i distinct variables in tree.\n", 1+max_var);
  nvars = max_var+1;
  free(vars);

  /* Setup libjit func signature */
  params = (jit_type_t *)malloc(nvars*sizeof(jit_type_t));
  for (i = 0; i < nvars; ++i) {
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
  for (i = 0; i < nvars; ++i) {
    var_values[i] = jit_value_get_param(function, i);
  }

  /* Recursively emit instructions for JIT and final return */
  jit_value_t rv = pj_jit_internal(function, var_values, nvars, term);
  jit_insn_return(function, rv);
  /*
  if (PJ_DEBUGGING)
    jit_dump_function(stdout, function, "func");
  */

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

