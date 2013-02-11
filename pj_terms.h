#ifndef PJ_TERMS_H_
#define PJ_TERMS_H_

typedef int pj_optype;

typedef enum {
  pj_ttype_constant,
  pj_ttype_variable,
  pj_ttype_op
} pj_term_type;

typedef enum {
  pj_binop_add,
  pj_binop_subtract,
  pj_binop_multiply,
  pj_binop_divide
} pj_op_type;


typedef struct {
  pj_optype type;
} pj_term_t;

typedef struct {
  pj_term_type type;
  pj_optype optype;
  pj_term_t *op1;
  pj_term_t *op2;
} pj_op_t;

typedef enum {
  pj_double_type,
  pj_int_type,
  pj_uint_type
} pj_basic_type;

typedef struct {
  pj_term_type type;
  pj_basic_type const_type;
  union {
    double dbl_value;
    int int_value;
    unsigned int uint_value;
  } value_u;
} pj_constant_t;

typedef struct {
  pj_term_type type;
  pj_basic_type var_type;
  int ivar;
} pj_variable_t;

pj_term_t *pj_make_const_dbl(double c);
pj_term_t *pj_make_const_int(int c);
pj_term_t *pj_make_const_uint(unsigned int c);
pj_term_t * pj_make_variable(int iv, pj_basic_type t);
pj_term_t * pj_make_binop(pj_optype t, pj_term_t *o1, pj_term_t *o2);

void pj_free_tree(pj_term_t *t);
void pj_dump_tree(pj_term_t *term);

#endif
