
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <jit/jit.h>

#include <pj_ast_terms.h>
#include <pj_ast_walkers.h>
#include <pj_ast_jit.h>

int
main(int argc, char **argv)
{
  pj_term_t *t;

  /* initialize tree structure */

  t = pj_make_binop(
    pj_binop_add,
    pj_make_unop(
      pj_unop_perl_int,
      pj_make_variable(0, pj_double_type)
    ),
    pj_make_variable(1, pj_double_type)
  );

  /* Just some debug output for the tree */
  pj_dump_tree(t);

  /* Setup JIT compiler */
  jit_context_t context;
  jit_function_t func = NULL;

  context = jit_context_create();

  /* Compile tree to function */
  pj_basic_type funtype;
  if (0 == pj_tree_jit(context, t, &func, &funtype)) {
    printf("JIT succeeded!\n");
  } else {
    printf("JIT failed!\n");
  }

  /* Setup function args */
  double arg1, arg2;
  void *args[2];
  double result;
  arg1 = -2.8;
  arg1 = 3.5;
  args[0] = &arg1;
  args[1] = &arg2;

  /* Call function (inefficiently) */
  jit_function_apply(func, args, &result);
  printf("foo(%f, %f) = %f\n", (float)arg1, (float)arg2, (float)result);

  void *cl = jit_function_to_closure(func);
  double (*fptr)(double, double) = cl;
  result = fptr(arg1, arg2);
  printf("foo(%f, %f) = %f\n", (float)arg1, (float)arg2, (float)result);

  /* format of the args data structure on pj_invoke_func is different from
   * jit_function_apply because we only support one argument type for now
   * anyway. */
  double args2[2];
  args2[0] = arg1;
  args2[1] = arg2;
  //int i;
  //for (i=0;i<1e8;++i){
  pj_invoke_func((pj_invoke_func_t)fptr, &args2, 2, funtype, (void *)&result);
  //}
  printf("foo(%f, %f) = %f\n", (float)arg1, (float)arg2, (float)result);

  /* Call function again, with slightly different input */
  arg2 = 3.8;
  jit_function_apply(func, args, &result);
  printf("foo(%f, %f) = %f\n", (float)arg1, (float)arg2, (float)result);

  /* cleanup */
  pj_free_tree(t);

  /* another example */
 
  t =  /*   pj_make_unop(
      pj_unop_not_bool,*/
      pj_make_binop(
      pj_binop_ne,
      pj_make_variable(0, pj_double_type),
      pj_make_const_dbl(0)
        )
	/*)*/
	;
  
  if (0 == pj_tree_jit(context, t, &func, &funtype)) {
    printf("JIT succeeded again!\n");
  } else {
    printf("JIT failed!\n");
  }
  pj_dump_tree(t);

  arg1 = 1.;
  arg2 = 0.;
  jit_function_apply(func, args, &result);
  printf("foo(%f, %f) = %f\n", (float)arg1, (float)arg2, (float)result);

  jit_context_destroy(context);
  return 0;
}

