#ifndef PJ_TERMS_H_
#define PJ_TERMS_H_

#include "types.h"

#include <vector>

// Definition of types and functions for the Perl JIT AST.
typedef struct op OP; // that's the Perl OP

typedef enum {
  pj_ttype_constant,
  pj_ttype_variable,
  pj_ttype_variabledeclaration,
  pj_ttype_optree,
  pj_ttype_nulloptree,
  pj_ttype_op
} pj_term_type;

typedef enum {
  pj_opc_unop,
  pj_opc_binop,
  pj_opc_listop
} pj_op_class;

// This file has the actual AST op enum declaration.
#include "pj_ast_ops_enum-gen.inc"

// Indicates that the given op will only evaluate its arguments
// conditionally (eg. short-circuiting boolean and/or, ternary).
#define PJ_ASTf_KIDS_CONDITIONAL (1<<0)
// Indicates that kids may or may not exist
#define PJ_ASTf_KIDS_OPTIONAL (1<<1)

namespace PerlJIT {
  namespace AST {
    class Term {
    public:
      Term(OP *p_op, pj_term_type t, Type *v_type = 0);

      pj_term_type type;
      OP *perl_op;

      virtual Type *get_value_type();
      virtual void set_value_type(Type *t);

      virtual void dump(int indent_lvl = 0) = 0;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Term"; }
      virtual ~Term();

    protected:
      Type *_value_type;
    };

    class Constant : public Term {
    public:
      Constant(OP *p_op, double c);
      Constant(OP *p_op, int c);
      Constant(OP *p_op, unsigned int c);

      union {
        double dbl_value;
        int int_value;
        unsigned int uint_value;
      };

      virtual void dump(int indent_lvl = 0);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Constant"; }
    };

    // abstract
    class Identifier : public Term {
    public:
      Identifier(OP *p_op, pj_term_type t, Type *v_type = 0);
    };

    class VariableDeclaration : public Identifier {
    public:
      VariableDeclaration(OP *p_op, int ivariable);

      int ivar;

      virtual void dump(int indent_lvl);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::VariableDeclaration"; }
    };

    // FIXME Right now, a Variable without declaration could reasonably be
    // any of the following: a package var, a lexically scoped thing (my/our)
    // from an outer sub (closures), ...
    class Variable : public Identifier {
    public:
      Variable(OP *p_op, VariableDeclaration *decl);

      VariableDeclaration *declaration;

      virtual Type *get_value_type() { return declaration->get_value_type(); }
      virtual void set_value_type(Type *t) { declaration->set_value_type(t); }

      virtual void dump(int indent_lvl);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Variable"; }
    };

    class Op : public Term {
    public:
      Op(OP *p_op, pj_op_type t);

      const char *name();
      unsigned int flags();
      virtual pj_op_class op_class() = 0;

      pj_op_type optype;
      std::vector<PerlJIT::AST::Term *> kids;

      virtual void dump(int indent_lvl = 0) = 0;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Op"; }
      virtual ~Op();
    };

    class Unop : public Op {
    public:
      Unop(OP *p_op, pj_op_type t, Term *kid);

      pj_op_class op_class()
        { return pj_opc_unop; }

      virtual void dump(int indent_lvl = 0);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Unop"; }
    };

    class Binop : public Op {
    public:
      Binop(OP *p_op, pj_op_type t, Term *kid1, Term *kid2);

      virtual void dump(int indent_lvl = 0);
      pj_op_class op_class()
        { return pj_opc_binop; }
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Binop"; }
    };

    class Listop : public Op {
    public:
      Listop(OP *p_op, pj_op_type t, const std::vector<Term *> &children);

      virtual void dump(int indent_lvl = 0);
      pj_op_class op_class()
        { return pj_opc_listop; }
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Listop"; }
    };

    class Optree : public Term {
    public:
      Optree(OP *p_op, OP *p_start_op);

      OP *start_op;
      virtual void dump(int indent_lvl = 0);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Optree"; }
    };

    class NullOptree : public Term {
    public:
      NullOptree(OP *p_op);

      virtual void dump(int indent_lvl = 0);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::NullOptree"; }
    };

  } // end namespace PerlJIT::AST
} // end namespace PerlJIT

#endif
