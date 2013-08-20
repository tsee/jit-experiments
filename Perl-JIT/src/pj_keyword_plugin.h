#ifndef PJ_KEYWORD_PLUGIN_H_
#define PJ_KEYWORD_PLUGIN_H_

#include <EXTERN.h>
#include <perl.h>

// Main keyword plugin hook for JIT type annotations
int pj_jit_type_keyword_plugin(pTHX_ char *keyword_ptr, STRLEN keyword_len, OP **op_ptr);

#define PJ_KEYWORD_PLUGIN_HINT "PJIT:kw"

#endif
