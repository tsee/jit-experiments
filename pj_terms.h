#ifndef PJ_TERMS_H_
#define PJ_TERMS_H_

typedef int pj_optype;

typedef enum {
  pj_t_constant,
  pj_t_variable,
  pj_t_binop
} pj_term_type;

typedef enum {
  pj_binop_add,
  pj_binop_subtract,
  pj_binop_multiply,
  pj_binop_divide
} pj_binop_type;


typedef struct {
  pj_optype type;
} pj_term_t;

typedef struct {
  pj_term_type type;
  pj_optype optype;
  pj_term_t *op1;
  pj_term_t *op2;
} pj_binop_t;

typedef struct {
  pj_term_type type;
  double value;
} pj_const_t;

typedef struct {
  pj_term_type type;
  int ivar;
} pj_var_t;

pj_const_t *pj_make_const(double c);
pj_var_t * pj_make_variable(int iv);
pj_binop_t * pj_make_binop(pj_optype t, pj_term_t *o1, pj_term_t *o2);

void pj_free_tree(pj_term_t *t);
void pj_dump_tree(pj_term_t *term);

#endif
