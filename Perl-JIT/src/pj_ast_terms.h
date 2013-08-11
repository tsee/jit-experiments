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

namespace PerlJIT {
  namespace AST {
    class Term {
    public:
      Term() {}
      Term(OP *p_op, pj_optype t) : type(t), perl_op(p_op) {}

      pj_optype type;
      OP *perl_op;

      void dump();

      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Term"; }
      virtual ~Term() {}
    };

    class Constant : public Term {
    public:
      Constant() {};
      Constant(OP *p_op, double c);
      Constant(OP *p_op, int c);
      Constant(OP *p_op, unsigned int c);

      pj_basic_type const_type;
      union {
        double dbl_value;
        int int_value;
        unsigned int uint_value;
      };

      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Constant"; }
    };

    class Variable : public Term {
    public:
      Variable() {}

      pj_basic_type var_type;
      int ivar;

      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Variable"; }
    };

    class Op : public Term {
    public:
      Op() {}

      pj_optype optype;
      std::vector<PerlJIT::AST::Term *> kids;

      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Op"; }
    };

    class Unop : public Op {
    public:
      Unop() {}
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Unop"; }
    };
    class Binop : public Op {
    public:
      Binop() {}
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Binop"; }
    };
    class Listop : public Op {
    public:
      Listop() {}
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Listop"; }
    };

  } // end namespace PerlJIT::AST
} // end namespace PerlJIT


PerlJIT::AST::Term *pj_make_variable(OP *perl_op, int iv, pj_basic_type t);
PerlJIT::AST::Term *pj_make_binop(OP *perl_op, pj_optype t, PerlJIT::AST::Term *o1, PerlJIT::AST::Term *o2);
PerlJIT::AST::Term *pj_make_unop(OP *perl_op, pj_optype t, PerlJIT::AST::Term *o1);
PerlJIT::AST::Term *pj_make_listop(OP *perl_op, pj_optype t, const std::vector<PerlJIT::AST::Term *> &children);
PerlJIT::AST::Term *pj_make_optree(OP *perl_op);

void pj_free_tree(PerlJIT::AST::Term *t);

/* purely a debugging aid! */
void pj_dump_tree(PerlJIT::AST::Term *term);

#endif
