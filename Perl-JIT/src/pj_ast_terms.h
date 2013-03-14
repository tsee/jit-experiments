#ifndef PJ_TERMS_H_
#define PJ_TERMS_H_

/* Definition of types and functions for the Perl JIT AST. */

typedef int pj_optype;

typedef enum {
  pj_ttype_constant,
  pj_ttype_variable,
  pj_ttype_op
} pj_term_type;

/* keep in sync with pj_ast_op_names in .c file */
typedef enum {
  pj_unop_negate,
  pj_unop_sin,
  pj_unop_cos,
  pj_unop_abs,
  pj_unop_sqrt,
  pj_unop_log,
  pj_unop_exp,
  pj_unop_perl_int, /* the equivalent to the perl int function */
  pj_unop_bitwise_not, /* TODO check */
  pj_unop_bool_not,

  pj_binop_add,
  pj_binop_subtract,
  pj_binop_multiply,
  pj_binop_divide,
  pj_binop_modulo,
  pj_binop_atan2,
  pj_binop_pow,
  pj_binop_left_shift, /* differs from perl for negative left operands */
  pj_binop_right_shift, /* differs from perl for negative left operands */
  pj_binop_bitwise_and, /* TODO check */
  pj_binop_bitwise_or, /* TODO check */
  pj_binop_bitwise_xor, /* TODO check */
  pj_binop_eq, /* TODO check */
  pj_binop_ne, /* TODO check */
  pj_binop_lt, /* TODO check */
  pj_binop_le, /* TODO check */
  pj_binop_gt, /* TODO check */
  pj_binop_ge, /* TODO check */
  pj_binop_bool_and,
  pj_binop_bool_or,

  /* TODO: more boolean operators, ternary */

  pj_unop_FIRST  = pj_unop_negate,
  pj_unop_LAST   = pj_unop_bool_not,

  pj_binop_FIRST = pj_binop_add,
  pj_binop_LAST  = pj_binop_bool_or
} pj_op_type;

typedef enum {
  pj_double_type,
  pj_int_type,
  pj_uint_type
} pj_basic_type;

/* Indicates that the given op will only evaluate its arguments
 * conditionally (eg. short-circuiting boolean and/or). */
#define PJ_ASTf_CONDITIONAL (1<<0)

extern unsigned int pj_ast_op_flags[];
#define PJ_OP_FLAGS(op) pj_ast_op_flags[(op)->type]

#define BASE_TERM_MEMBERS   \
  pj_optype type;           \
  pj_term_t *op_sibling;

typedef struct pj_term_t pj_term_t;
struct pj_term_t {
  BASE_TERM_MEMBERS
};

typedef struct {
  BASE_TERM_MEMBERS
  pj_optype optype;
  pj_term_t *op1;
  pj_term_t *op2;
} pj_op_t;

typedef struct {
  BASE_TERM_MEMBERS
  pj_basic_type const_type;
  union {
    double dbl_value;
    int int_value;
    unsigned int uint_value;
  } value_u;
} pj_constant_t;

typedef struct {
  BASE_TERM_MEMBERS
  pj_basic_type var_type;
  int ivar;
} pj_variable_t;


pj_term_t *pj_make_const_dbl(double c);
pj_term_t *pj_make_const_int(int c);
pj_term_t *pj_make_const_uint(unsigned int c);
pj_term_t *pj_make_variable(int iv, pj_basic_type t);
pj_term_t *pj_make_binop(pj_optype t, pj_term_t *o1, pj_term_t *o2);
pj_term_t *pj_make_unop(pj_optype t, pj_term_t *o1);

void pj_free_tree(pj_term_t *t);

/* purely a debugging aid! */
void pj_dump_tree(pj_term_t *term);

#endif
