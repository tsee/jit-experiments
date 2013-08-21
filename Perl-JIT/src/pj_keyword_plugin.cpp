#include "pj_keyword_plugin.h"
#include "pj_global_state.h"
#include "types.h"
#include "OPTreeVisitor.h"
#include <iostream>
#include <string>

using namespace PerlJIT;
using namespace std;

STATIC string
S_parse_identifier(pTHX)
{
  char *start = PL_parser->bufptr;
  while (1) {
    char * const end = PL_parser->bufend;
    char *s = PL_parser->bufptr;
    while (end-s >= 1) {
      if (!isWORDCHAR(*s)) {
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
  return string("");
}


STATIC string
S_parse_varname(pTHX)
{
  I32 c;

  string varname;

  c = lex_read_unichar(LEX_KEEP_PREVIOUS);
  switch (c) {
  case '$':
  case '@':
  case '%':
    varname.append(1, (char)c);
    break;
  default:
    return string("");
  }

  varname.append(S_parse_identifier(aTHX));

  if (varname.length() == 1)
    return string("");
  return varname;
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

  // This isn't the right way - it does a general list expr parse :(
  OP *parsed_optree = parse_barestmt(0);
  //OP *parsed_optree = parse_listexpr(0);
  *op_ptr = parsed_optree;
  op_dump(parsed_optree);

//  lex_read_space(0);
//  c = lex_peek_unichar(0);
//  if (c == '(')
//    croak("FIXME list declarations not implemented yet!");

//  string varname = S_parse_varname(aTHX);
//  cout << "'" << type_str << "' '" << varname << "'"<< endl;
//  char *s = PL_parser->bufptr;
//  printf("'%s'\n", s);
//  abort();

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

