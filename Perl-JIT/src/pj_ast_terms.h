#ifndef PJ_TERMS_H_
#define PJ_TERMS_H_

#include <vector>

/* Definition of types and functions for the Perl JIT AST. */
typedef struct op OP;
typedef int pj_optype;

typedef enum {
  pj_ttype_constant,
  pj_ttype_variable,
  pj_ttype_optree,
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

  pj_listop_ternary, /* TODO check */
  /* TODO: more boolean operators, ternary */

  pj_unop_FIRST  = pj_unop_negate,
  pj_unop_LAST   = pj_unop_bool_not,

  pj_binop_FIRST = pj_binop_add,
  pj_binop_LAST  = pj_binop_bool_or,

  pj_listop_FIRST = pj_listop_ternary,
  pj_listop_LAST  = pj_listop_ternary,
} pj_op_type;

#define PJ_IS_OP_UNOP(o) ((o)->optype >= pj_unop_FIRST && (o)->optype <= pj_unop_LAST)
#define PJ_IS_OP_BINOP(o) ((o)->optype >= pj_binop_FIRST && (o)->optype <= pj_binop_LAST)
#define PJ_IS_OP_LISTOP(o) ((o)->optype >= pj_listop_FIRST && (o)->optype <= pj_listop_LAST)

typedef enum {
  pj_double_type,
  pj_int_type,
  pj_uint_type
} pj_basic_type;

/* Indicates that the given op will only evaluate its arguments
 * conditionally (eg. short-circuiting boolean and/or). */
#define PJ_ASTf_CONDITIONAL (1<<0)

extern unsigned int pj_ast_op_flags[];
#define PJ_OP_FLAGS(op) (pj_ast_op_flags[(op)->optype])
extern const char *pj_ast_op_names[];
#define PJ_OP_NAME(op) (pj_ast_op_names[(op)->optype])

struct pj_term_t {
  pj_optype type;
  OP *perl_op;

  virtual const char *perl_class() const
    { return "Perl::JIT::AST::Term"; }
};

struct pj_op_t : public pj_term_t {
  pj_optype optype;
  std::vector<pj_term_t *> kids;

  virtual const char *perl_class() const
    { return "Perl::JIT::AST::Op"; }
};

struct pj_unop_t : public pj_op_t {
  virtual const char *perl_class() const
    { return "Perl::JIT::AST::Unop"; }
};
struct pj_binop_t : public pj_op_t {
  virtual const char *perl_class() const
    { return "Perl::JIT::AST::Binop"; }
};
struct pj_listop_t : public pj_op_t {
  virtual const char *perl_class() const
    { return "Perl::JIT::AST::Listop"; }
};

struct pj_constant_t : public pj_term_t {
  pj_basic_type const_type;
  union {
    double dbl_value;
    int int_value;
    unsigned int uint_value;
  };

  virtual const char *perl_class() const
    { return "Perl::JIT::AST::Constant"; }
};

struct pj_variable_t : public pj_term_t {
  pj_basic_type var_type;
  int ivar;

  virtual const char *perl_class() const
    { return "Perl::JIT::AST::Variable"; }
};


pj_term_t *pj_make_const_dbl(OP *perl_op, double c);
pj_term_t *pj_make_const_int(OP *perl_op, int c);
pj_term_t *pj_make_const_uint(OP *perl_op, unsigned int c);
pj_term_t *pj_make_variable(OP *perl_op, int iv, pj_basic_type t);
pj_term_t *pj_make_binop(OP *perl_op, pj_optype t, pj_term_t *o1, pj_term_t *o2);
pj_term_t *pj_make_unop(OP *perl_op, pj_optype t, pj_term_t *o1);
pj_term_t *pj_make_listop(OP *perl_op, pj_optype t, const std::vector<pj_term_t *> &children);
pj_term_t *pj_make_optree(OP *perl_op);

void pj_free_tree(pj_term_t *t);

/* purely a debugging aid! */
void pj_dump_tree(pj_term_t *term);

#endif
