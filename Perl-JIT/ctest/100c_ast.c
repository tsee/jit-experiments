
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <jit/jit.h>

#include <pj_ast_terms.h>
#include <pj_ast_walkers.h>
#include <pj_ast_jit.h>

#include "mytap.h"

void basic_term_tests();

int
main ()
{
  basic_term_tests();

  ok_m(1, "alive at end");
  done_testing();
  return 0;
}

#define TEST_ARGS_MAX 20

void
basic_term_tests()
{
  const unsigned int ntests = 9;
  pj_term_t *test_tree[ntests];
  unsigned int test_inputcount[ntests];
  double *test_input[ntests];
  double test_output[ntests];
  char *test_name[ntests];
  unsigned int i;


  i = 0;
  test_name[i] = "negate float variable";
  test_inputcount[i] = 1;
  test_tree[i] = pj_make_unop(
    pj_unop_negate,
    pj_make_variable(0, pj_double_type)
  );
  test_input[i] = (double *)malloc(sizeof(double)*1);
  test_input[i][0] = 3.1;
  test_output[i] = -3.1;

  i = 1;
  test_name[i] = "multiply float variable with constant";
  test_inputcount[i] = 1;
  test_tree[i] = pj_make_binop(
    pj_binop_multiply,
    pj_make_variable(0, pj_double_type),
    pj_make_const_dbl(2.1)
  );
  test_input[i] = (double *)malloc(sizeof(double)*1);
  test_input[i][0] = -5;
  test_output[i] = (-5) * 2.1;

  i = 2;
  test_name[i] = "($a && 2.1), $a == 0";
  test_inputcount[i] = 1;
  test_tree[i] = pj_make_binop(
    pj_binop_bool_and,
    pj_make_variable(0, pj_double_type),
    pj_make_const_dbl(2.1)
  );
  test_input[i] = (double *)malloc(sizeof(double)*1);
  test_input[i][0] = 0.;
  test_output[i] = 0.;

  i = 3;
  test_name[i] = "($a && 2.1), $a == 3.3";
  test_inputcount[i] = 1;
  test_tree[i] = pj_make_binop(
    pj_binop_bool_and,
    pj_make_variable(0, pj_double_type),
    pj_make_const_dbl(2.1)
  );
  test_input[i] = (double *)malloc(sizeof(double)*1);
  test_input[i][0] = 3.3;
  test_output[i] = 2.1;

  i = 4;
  test_name[i] = "($a && 0), $a == 0";
  test_inputcount[i] = 1;
  test_tree[i] = pj_make_binop(
    pj_binop_bool_and,
    pj_make_variable(0, pj_double_type),
    pj_make_const_dbl(0.0)
  );
  test_input[i] = (double *)malloc(sizeof(double)*1);
  test_input[i][0] = 0.0;
  test_output[i] = 0.0;

  i = 5;
  test_name[i] = "($a || 2.1), $a == 0";
  test_inputcount[i] = 1;
  test_tree[i] = pj_make_binop(
    pj_binop_bool_or,
    pj_make_variable(0, pj_double_type),
    pj_make_const_dbl(2.1)
  );
  test_input[i] = (double *)malloc(sizeof(double)*1);
  test_input[i][0] = 0.;
  test_output[i] = 2.1;

  i = 6;
  test_name[i] = "($a || 2.1), $a == 3.3";
  test_inputcount[i] = 1;
  test_tree[i] = pj_make_binop(
    pj_binop_bool_or,
    pj_make_variable(0, pj_double_type),
    pj_make_const_dbl(2.1)
  );
  test_input[i] = (double *)malloc(sizeof(double)*1);
  test_input[i][0] = 3.3;
  test_output[i] = 3.3;

  i = 7;
  test_name[i] = "($a ? 0.1 : 1.1), $a == 0.0";
  test_inputcount[i] = 1;
  {
    pj_term_t *v = pj_make_variable(0, pj_double_type);
    pj_term_t *c1 = pj_make_const_dbl(0.1);
    pj_term_t *c2 = pj_make_const_dbl(1.1);
    v->op_sibling = c1;
    c1->op_sibling = c2;
    test_tree[i] = pj_make_listop(pj_listop_ternary, v, c2);
  }
  test_input[i] = (double *)malloc(sizeof(double)*1);
  test_input[i][0] = 0.0;
  test_output[i] = 1.1;

  i = 8;
  test_name[i] = "($a ? 0.1 : 1.1), $a == 3.0";
  test_inputcount[i] = 1;
  {
    pj_term_t *v = pj_make_variable(0, pj_double_type);
    pj_term_t *c1 = pj_make_const_dbl(0.1);
    pj_term_t *c2 = pj_make_const_dbl(1.1);
    v->op_sibling = c1;
    c1->op_sibling = c2;
    test_tree[i] = pj_make_listop(pj_listop_ternary, v, c2);
  }
  test_input[i] = (double *)malloc(sizeof(double)*1);
  test_input[i][0] = 3.0;
  test_output[i] = 0.1;

  for (i = 0; i < ntests; ++i) {
    jit_context_t context;
    pj_basic_type funtype;
    jit_function_t func = NULL;
    void *closure;
    double result = 0.;
    char namebuf[1024];

    context = jit_context_create();

    /* Compile tree to function */
    sprintf(namebuf, "%s, JIT succeeded", test_name[i]);
    ok_m(0 == pj_tree_jit(context, test_tree[i], &func, &funtype), namebuf);

    closure = jit_function_to_closure(func);
    pj_invoke_func((pj_invoke_func_t)closure, test_input[i], test_inputcount[i], funtype, (void *)&result);
    sprintf(namebuf, "%s, result correct", test_name[i]);
    is_double_m(1e-9, result, test_output[i], namebuf);

    jit_context_destroy(context);

    pj_free_tree(test_tree[i]);
    free(test_input[i]);
  }

}

