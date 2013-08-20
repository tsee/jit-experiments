#include "pj_keyword_plugin.h"
#include "pj_global_state.h"
#include "types.h"
#include "OPTreeVisitor.h"

#include <iostream>
#include <string>
#include <vector>
#include <tr1/unordered_map>

#include <EXTERN.h>
#include <perl.h>

using namespace PerlJIT;
using namespace std;
using namespace std::tr1;

namespace PerlJIT {
  class PadSvDeclarationOpExtractor: public OPTreeVisitor {
  public:
    OPTreeVisitor::visit_control_t visit_op(pTHX_ OP *o, OP *parentop);

    const vector<OP *> & get_padsv_ops()
      { return fPadSVOPs; }

  private:
    vector<OP *> fPadSVOPs;
  };
}

OPTreeVisitor::visit_control_t
PadSvDeclarationOpExtractor::visit_op(pTHX_ OP *o, OP *parentop)
{
  if (o->op_type == OP_PADSV && o->op_flags & OPpLVAL_INTRO)
    fPadSVOPs.push_back(o);
  return VISIT_CONT;
}

STATIC int
pj_type_annotate_mg_free(pTHX_ SV *sv, MAGIC* mg)
{
  PERL_UNUSED_ARG(sv);
  unordered_map<PADOFFSET, TypedPadSvOp> *decl_map
    = (unordered_map<PADOFFSET, TypedPadSvOp>*)mg->mg_ptr;
  delete decl_map;
  mg->mg_ptr = NULL;
}


STATIC MGVTBL PJ_type_annotate_mg_vtbl = {
  0, 0, 0, 0, pj_type_annotate_mg_free, 0, 0, 0
};


unordered_map<PADOFFSET, TypedPadSvOp> *
pj_get_typed_variable_declarations(pTHX_ CV *cv)
{
  MAGIC *mg = mg_findext((SV *)cv, PERL_MAGIC_ext, &PJ_type_annotate_mg_vtbl);
  if (mg != NULL) {
    return (unordered_map<PADOFFSET, TypedPadSvOp> *)mg->mg_ptr;
  }
  else {
    return NULL;
  }
}


STATIC string
S_lex_to_whitespace(pTHX)
{
  char *start = PL_parser->bufptr;
  while (1) {
    char * const end = PL_parser->bufend;
    char *s = PL_parser->bufptr;
    while (end-s >= 1) {
      if (isSPACE(*s)) {
        lex_read_to(s); /* skip Perl's lexer/parser ahead to end of type */
        return string(start, (size_t)(s-start));
      }
      s++;
    }
    if ( !lex_next_chunk(LEX_KEEP_PREVIOUS) ) {
      lex_read_to(s); /* skip Perl's lexer/parser ahead to end of type */
      lex_read_space(0);
      return string(""); /* end of code */
    }
  }
}

// typed <type> SCALAR
// typed <type> (DECL-LIST)
STATIC void
S_parse_typed_declaration(pTHX_ OP **op_ptr)
{
  I32 c;

  // need space before type
  c = lex_peek_unichar(0);
  if (c < 0 || !isSPACE(c))
    croak("syntax error");
  lex_read_space(0);

  c = lex_peek_unichar(0);
  if (c < 0 || isSPACE(c))
    croak("syntax error");
  // inch our way forward to end-of-type
  string type_str =  S_lex_to_whitespace(aTHX);
  if (type_str == string(""))
    croak("syntax error while extracting variable type");

  AST::Type *type = AST::parse_type(type_str);
  if (type == NULL)
    croak("syntax error '%s' does not name a type", type_str.c_str());

  // Skip space (which we know to exist from S_lex_to_whitespace
  lex_read_space(0);

  // Oh man, this is so wrong. Secretly inject a bit of code into the
  // parse so that we can use Perl's parser to grok the declaration.
  lex_stuff_pvs(" my ", 0);

  // Let Perl parse the rest of the statement.
  OP *parsed_optree = parse_barestmt(0);

  // Get the actual declaration OPs
  PadSvDeclarationOpExtractor extractor;
  extractor.visit(aTHX_ parsed_optree, NULL);
  const vector<OP *> &declaration_ops = extractor.get_padsv_ops();

  // Get existing declarations or create new container
  unordered_map<PADOFFSET, TypedPadSvOp> *decl_map;
  decl_map = pj_get_typed_variable_declarations(aTHX_ PL_compcv);
  if (decl_map == NULL) {
    decl_map = new unordered_map<PADOFFSET, TypedPadSvOp>;
    (void)sv_magicext(
      (SV *)PL_compcv, NULL, PERL_MAGIC_ext, &PJ_type_annotate_mg_vtbl,
      (char *)decl_map, 0
    );
  }

  const unsigned int ndecl = declaration_ops.size();
  for (unsigned int i = 0; i < ndecl; ++i) {
    (*decl_map)[declaration_ops[i]->op_targ] = TypedPadSvOp(type, declaration_ops[i]);
  }

  // Delete any unowned type
  if (decl_map->size() == 0)
    delete type;
  else
    (*decl_map)[declaration_ops.back()->op_targ].set_type_ownership(true);

  *op_ptr = parsed_optree;
}


// Main keyword plugin hook for JIT type annotations
int
pj_jit_type_keyword_plugin(pTHX_ char *keyword_ptr, STRLEN keyword_len, OP **op_ptr)
{
  /* Enforce lexical scope of this keyword plugin */
  HV *hints;
  if (!(hints = GvHV(PL_hintgv)))
    return FALSE;
  if (!(hv_fetchs(hints, PJ_KEYWORD_PLUGIN_HINT, 0)))
    return FALSE;

  int ret;
  if (keyword_len == 5 && memcmp(keyword_ptr, "typed", 5) == 0)
  {
    SAVETMPS;
    S_parse_typed_declaration(aTHX_ op_ptr); // sets *op_ptr or croaks
    ret = KEYWORD_PLUGIN_EXPR;
    //ret = KEYWORD_PLUGIN_STMT;
    FREETMPS;
  }
  else {
    ret = (*PJ_next_keyword_plugin)(aTHX_ keyword_ptr, keyword_len, op_ptr);
  }

  return ret;
}

