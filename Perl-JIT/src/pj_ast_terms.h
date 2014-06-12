#ifndef PJ_TERMS_H_
#define PJ_TERMS_H_

#include "pj_types.h"

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
  pj_ttype_statement,
  pj_ttype_bareblock,
  pj_ttype_while,
  pj_ttype_for,
  pj_ttype_foreach,
  pj_ttype_map,
  pj_ttype_grep,
  pj_ttype_function_call,
  pj_ttype_empty,
  pj_ttype_loop_control,
  pj_ttype_sort
} pj_term_type;

typedef enum {
  pj_opc_baseop,
  pj_opc_unop,
  pj_opc_binop,
  pj_opc_listop,
  pj_opc_block
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
  pj_sigil_glob,
  pj_sigil_code
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

      OP *start_op();
      virtual OP *first_op();
      virtual OP *last_op();

      pj_op_context context() const;

      pj_term_type get_type() const;
      void set_type(const pj_term_type t);

      OP *get_perl_op() const;
      void set_perl_op(OP *p_op);

      virtual Type *get_value_type() const;
      virtual void set_value_type(Type *t);

      virtual std::vector<Term *> get_kids() const { return empty; }

      virtual void dump(int indent_lvl = 0) const = 0;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Term"; }
      virtual ~Term();

    protected:
      pj_term_type type;
      OP *perl_op;
      Type *_value_type;

      static const std::vector<Term *> empty;
    };

    class Empty : public Term {
    public:
      Empty();

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Empty"; }
    };

    // A relatively low-on-semantics group of things, such as for
    // having separate lists of things for aassign or list slice (list
    // and indices in the latter)
    class List : public Term {
    public:
      List();
      List(const std::vector<Term *> &kid_terms);

      std::vector<Term *> get_kids() const { return kids; }
      std::vector<PerlJIT::AST::Term *> kids;

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::List"; }
      virtual ~List();
    };

    class Constant : public Term {
    public:
      Constant(OP *p_op, Type *v_type);

      virtual void dump(int indent_lvl = 0) const = 0;
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

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::NumericConstant"; }
    };

    class StringConstant : public Constant {
    public:
      StringConstant(OP *p_op, const std::string &s, bool isUTF8);
      StringConstant(pTHX_ OP *p_op, SV *string_literal_sv);

      std::string string_value;
      bool is_utf8;

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::StringConstant"; }
    };

    class UndefConstant : public Constant {
    public:
      UndefConstant();

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::UndefConstant"; }
    };

    class ArrayConstant : public Constant {
    public:
      ArrayConstant(OP *p_op, AV *array);

      AV *get_const_array() const;

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::ArrayConstant"; }

    private:
      AV *const_array;
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

      int get_pad_index() const { return perl_op->op_targ; }

      virtual void dump(int indent_lvl) const;
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

      virtual Type *get_value_type() const;
      virtual void set_value_type(Type *t);

      virtual void dump(int indent_lvl) const;
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

      virtual void dump(int indent_lvl) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Global"; }
    };

    class Op : public Term {
    public:
      Op(OP *p_op, pj_op_type t);

      virtual const char *name() const;
      unsigned int flags() const;
      virtual pj_op_class op_class() const = 0;

      std::vector<Term *> get_kids() const { return kids; }

      bool evaluates_kids_conditionally() const
        { return flags() & PJ_ASTf_KIDS_CONDITIONAL; }

      bool kids_are_optional() const
        { return flags() & PJ_ASTf_KIDS_OPTIONAL; }

      bool may_have_explicit_overload() const
        { return flags() & PJ_ASTf_OVERLOAD; }

      std::vector<PerlJIT::AST::Term *> kids;

      pj_op_type get_op_type() const;
      void set_op_type(pj_op_type t);

      bool is_integer_variant() const;
      void set_integer_variant(bool is_integer_variant);

      virtual void dump(int indent_lvl = 0) const = 0;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Op"; }
      virtual ~Op();

    protected:
      pj_op_type op_type;
      bool _is_integer_variant;
    };

    class Baseop : public Op {
    public:
      Baseop(OP *p_op, pj_op_type t);

      pj_op_class op_class() const
        { return pj_opc_baseop; }

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Baseop"; }
    };

    class Unop : public Op {
    public:
      Unop(OP *p_op, pj_op_type t, Term *kid);

      pj_op_class op_class() const
        { return pj_opc_unop; }

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Unop"; }
    };

    class Binop : public Op {
    public:
      Binop(OP *p_op, pj_op_type t, Term *kid1, Term *kid2);

      bool is_assignment_form() const;
      void set_assignment_form(bool is_assignment);
      bool is_synthesized_assignment() const;

      virtual void dump(int indent_lvl = 0) const;
      pj_op_class op_class() const
        { return pj_opc_binop; }
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Binop"; }

    private:
      bool is_assign_form;
    };

    class Listop : public Op {
    public:
      Listop(OP *p_op, pj_op_type t, const std::vector<Term *> &children);

      virtual void dump(int indent_lvl = 0) const;
      pj_op_class op_class() const
        { return pj_opc_listop; }
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Listop"; }
    };

    class Block : public Op {
    public:
      Block(OP *p_op, Term *statements);

      virtual void dump(int indent_lvl = 0) const;
      pj_op_class op_class() const
        { return pj_opc_block; }
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Block"; }
    };

    class BareBlock : public Term {
    public:
      BareBlock(OP *p_op, Term *body, Term *continuation);

      Term *get_body() const;
      Term *get_continuation() const;

      std::vector<PerlJIT::AST::Term *> get_kids() const;

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::BareBlock"; }

    protected:
      Term *body;
      Term *continuation;
    };

    class While : public Term {
    public:
      While(OP *p_op, Term *condition, bool negated, bool evaluate_after, Term *body, Term *continuation);

      Term *condition;
      bool negated, evaluate_after;
      Term *body;
      Term *continuation;

      std::vector<PerlJIT::AST::Term *> get_kids() const;

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::While"; }
    };

    class For : public Term {
    public:
      For(OP *p_op, Term *init, Term *condition, Term *step, Term *body);

      Term *init;
      Term *condition;
      Term *step;
      Term *body;

      virtual OP *last_op();

      std::vector<PerlJIT::AST::Term *> get_kids() const;

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::For"; }
    };

    class Foreach : public Term {
    public:
      Foreach(OP *p_op, Term *iterator, Term *expression, Term *body, Term *continuation);

      Term *iterator;
      Term *expression;
      Term *body;
      Term *continuation;

      std::vector<PerlJIT::AST::Term *> get_kids() const;

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Foreach"; }
    };

    class ListTransformation : public Term {
    public:
      ListTransformation(OP *p_op, pj_term_type _type, Term *body, List *parameters);
      Term *body;
      List *parameters;

      std::vector<PerlJIT::AST::Term *> get_kids();

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::ListTransformation"; }
    };

    class Map : public ListTransformation {
    public:
      Map(OP *p_op, Term *body, List *parameters);

      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Map"; }
    };

    class Grep : public ListTransformation {
    public:
      Grep(OP *p_op, Term *body, List *parameters);

      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Grep"; }
    };

    class SubCall : public Term {
    public:
      SubCall(OP *entersub_op,
              PerlJIT::AST::Term *cv_src,
              const std::vector<PerlJIT::AST::Term *> &args);

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::SubCall"; }

      std::vector<PerlJIT::AST::Term *> get_arguments() const;
      PerlJIT::AST::Term *get_cv_source() const;

    private:
      PerlJIT::AST::Term *_cv_source;
      std::vector<PerlJIT::AST::Term *> _arguments;
    };

    class MethodCall : public SubCall {
    public:
      MethodCall(OP *entersub_op,
                 PerlJIT::AST::Term *cv_src,
                 PerlJIT::AST::Term *invocant,
                 const std::vector<PerlJIT::AST::Term *> &args);

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::MethodCall"; }

      PerlJIT::AST::Term *get_invocant() const;

    private:
      PerlJIT::AST::Term *_invocant;
    };

    class LoopControlStatement : public Term {
    public:
      enum pj_loop_ctl_type {
        pj_lctl_next,
        pj_lctl_redo,
        pj_lctl_last
      };

      LoopControlStatement(pTHX_ OP *p_op, AST::Term *kid);

      pj_loop_ctl_type get_loop_ctl_type() const { return ctl_type; }
      std::vector<Term *> get_kids() const { return kids; }

      bool has_label() const { return _has_label; }
      bool label_is_dynamic() const { return _label_is_dynamic; }
      bool label_is_utf8() const { return _label_is_utf8; }
      const std::string &get_label() const { return label; }

      PerlJIT::AST::Term *get_jump_target() const
      { return jump_target; }
      void set_jump_target(PerlJIT::AST::Term *target)
      { jump_target = target; }

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::LoopControlStatement"; }

    private:
      void init_label(pTHX);

      pj_loop_ctl_type ctl_type;
      std::vector<PerlJIT::AST::Term *> kids;
      PerlJIT::AST::Term *jump_target;
      bool _has_label;
      bool _label_is_dynamic;
      bool _label_is_utf8;
      std::string label;
    };

    class Sort : public Term {
    public:
      Sort(OP *p_op, bool reverse, bool std_numeric, Term *cmp_fun, const std::vector<Term *> & args);

      bool is_reverse_sort() const { return needs_reverse; }
      bool is_std_numeric_sort() const { return is_numeric; }
      Term *get_cmp_function() const { return cmp_function; }
      const std::vector<Term *> & get_arguments() const { return arguments; }

      void set_reverse_sort(bool is_reverse) { needs_reverse = is_reverse; }

      void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Sort"; }

    protected:
      bool needs_reverse;
      bool is_numeric;
      Term *cmp_function;
      std::vector<Term *> arguments;
    };

    class Optree : public Term {
    public:
      Optree(OP *p_op);

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Optree"; }
    };

    class NullOptree : public Term {
    public:
      NullOptree(OP *p_op);

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::NullOptree"; }
    };

    class Statement : public Term {
    public:
      Statement(OP *p_nextstate, Term *term);

      std::vector<PerlJIT::AST::Term *> kids;

      std::vector<Term *> get_kids() const { return kids; }

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Statement"; }
    };

    // This class is anomalous in multiple ways: it does not really map to
    // a complete syntactic construct, its perl_op member is NULL and
    // it's created outside normal op traversal
    // It's an unfortunate necessity until we can recognize (and have AST
    // classes for) all block statements
    class StatementSequence : public Term {
    public:
      StatementSequence();

      std::vector<PerlJIT::AST::Term *> kids;

      std::vector<Term *> get_kids() const { return kids; }

      virtual void dump(int indent_lvl = 0) const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::StatementSequence"; }
    };
  } // end namespace PerlJIT::AST
} // end namespace PerlJIT

#endif
