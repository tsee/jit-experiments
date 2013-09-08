#ifndef PJ_TERMS_H_
#define PJ_TERMS_H_

#include "types.h"

#include <vector>
#include <string>

#include <EXTERN.h>
#include <perl.h>

// Definition of types and functions for the Perl JIT AST.
typedef struct op OP; // that's the Perl OP

typedef enum {
  pj_ttype_constant,
  pj_ttype_lexical,
  pj_ttype_global,
  pj_ttype_variabledeclaration,
  pj_ttype_list,
  pj_ttype_optree,
  pj_ttype_nulloptree,
  pj_ttype_op,
  pj_ttype_statementsequence,
  pj_ttype_statement
} pj_term_type;

typedef enum {
  pj_opc_unop,
  pj_opc_binop,
  pj_opc_listop
} pj_op_class;

// These fly in close formation with the OPf_WANT* flags in core
// in that pj_context_void/scalar/list map to the corresponding flags in
// their numeric values 1/2/3 or else using OP_GIMME wouldn't work.
typedef enum {
  pj_context_caller,
  pj_context_void,
  pj_context_scalar,
  pj_context_list
} pj_op_context;

typedef enum {
  pj_sigil_scalar,
  pj_sigil_array,
  pj_sigil_hash,
  pj_sigil_glob
} pj_variable_sigil;

// This file has the actual AST op enum declaration.
#include "pj_ast_ops_enum-gen.inc"

// Indicates that the given op will only evaluate its arguments
// conditionally (eg. short-circuiting boolean and/or, ternary).
#define PJ_ASTf_KIDS_CONDITIONAL (1<<0)
// Indicates that kids may or may not exist
#define PJ_ASTf_KIDS_OPTIONAL (1<<1)
// Indicates that the op has an assignment form (e.g. +=, &&=, ...)
#define PJ_ASTf_HAS_ASSIGNMENT_FORM (1<<2)
// Indicates that the op may have overloading
#define PJ_ASTf_OVERLOAD (1<<3)

namespace PerlJIT {
  namespace AST {
    class Term {
    public:
      Term(OP *p_op, pj_term_type t, Type *v_type = 0);

      pj_term_type type;
      OP *perl_op;

      OP *start_op();

      pj_op_context context();

      virtual Type *get_value_type();
      virtual void set_value_type(Type *t);

      virtual void dump(int indent_lvl = 0) = 0;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Term"; }
      virtual ~Term();

    protected:
      Type *_value_type;
    };

    // A relatively low-on-semantics group of things, such as for
    // having separate lists of things for aassign or list slice (list
    // and indices in the latter)
    class List : public Term {
    public:
      List();
      List(const std::vector<Term *> &kid_terms);

      std::vector<PerlJIT::AST::Term *> kids;

      virtual void dump(int indent_lvl = 0);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::List"; }
      virtual ~List();
    };

    class Constant : public Term {
    public:
      Constant(OP *p_op, Type *v_type);

      virtual void dump(int indent_lvl = 0) = 0;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Constant"; }
    };

    class NumericConstant : public Constant {
    public:
      NumericConstant(OP *p_op, NV c);
      NumericConstant(OP *p_op, IV c);
      NumericConstant(OP *p_op, UV c);

      union {
        double dbl_value;
        int int_value;
        unsigned int uint_value;
      };

      virtual void dump(int indent_lvl = 0);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::NumericConstant"; }
    };

    class StringConstant : public Constant {
    public:
      StringConstant(OP *p_op, const std::string &s, bool isUTF8);
      StringConstant(pTHX_ OP *p_op, SV *string_literal_sv);

      std::string string_value;
      bool is_utf8;

      virtual void dump(int indent_lvl = 0);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::StringConstant"; }
    };

    class UndefConstant : public Constant {
    public:
      UndefConstant();

      virtual void dump(int indent_lvl = 0);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::UndefConstant"; }
    };

    // abstract
    class Identifier : public Term {
    public:
      Identifier(OP *p_op, pj_term_type t, pj_variable_sigil s, Type *v_type = 0);

      pj_variable_sigil sigil;
    };

    class VariableDeclaration : public Identifier {
    public:
      VariableDeclaration(OP *p_op, int ivariable, pj_variable_sigil s, Type *v_type = 0);

      int ivar;

      virtual void dump(int indent_lvl);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::VariableDeclaration"; }
    };

    // FIXME Right now, a Variable without declaration could reasonably be
    // any of the following: a package var, a lexically scoped thing (my/our)
    // from an outer sub (closures), ...
    class Lexical : public Identifier {
    public:
      Lexical(OP *p_op, VariableDeclaration *decl);

      VariableDeclaration *declaration;

      int get_pad_index() const;

      virtual Type *get_value_type() { return declaration->get_value_type(); }
      virtual void set_value_type(Type *t) { declaration->set_value_type(t); }

      virtual void dump(int indent_lvl);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Lexical"; }
    };

    class Global : public Identifier {
    public:
      Global(OP *p_op, pj_variable_sigil sigil);

#ifdef USE_ITHREADS
      int get_pad_index() const;
#else
      GV *get_gv() const;
#endif

      virtual void dump(int indent_lvl);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Global"; }
    };

    class Op : public Term {
    public:
      Op(OP *p_op, pj_op_type t);

      virtual const char *name();
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

      bool is_assignment_form();
      void set_assignment_form(bool is_assignment);
      bool is_synthesized_assignment() const;

      virtual void dump(int indent_lvl = 0);
      pj_op_class op_class()
        { return pj_opc_binop; }
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Binop"; }

    private:
      bool is_assign_form;
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
      Optree(OP *p_op);

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

    class Statement : public Term {
    public:
      Statement(OP *p_nextstate, Term *term);

      std::vector<PerlJIT::AST::Term *> kids;

      virtual void dump(int indent_lvl = 0);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Statement"; }
    };

    // This class is anomalous in multiple ways: it does not rellay map to
    // a complete syntactic construct, its perl_op member is NULL and
    // it's created outside normal op traversal
    // It's an unfortunate necessity until we can recognize (and have AST
    // classes for) all block statements
    class StatementSequence : public Term {
    public:
      StatementSequence();

      std::vector<PerlJIT::AST::Term *> kids;

      virtual void dump(int indent_lvl = 0);
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::StatementSequence"; }
    };
  } // end namespace PerlJIT::AST
} // end namespace PerlJIT

#endif
