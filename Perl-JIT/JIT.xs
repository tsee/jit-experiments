/*
# vim: ts=2 sw=2 et
*/
#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include <assert.h>

#include "stack.h"

#include <jit/jit.h>

#include "pj_debug.h"

/* AST and AST -> C fun stuff */
#include "pj_ast_terms.h"
#include "pj_ast_jit.h"

/* OP-tree walker logic */
#include "pj_optree.h"

/* Global state and initialization routines */
#include "pj_global_state.h"

/* Everything related to the actual run-time custom OP implementation */
#include "pj_jit_op.h"

/* The custom peephole optimizer routines */
#include "pj_jit_peep.h"


MODULE = Perl::JIT	PACKAGE = Perl::JIT

BOOT:
    pj_init_global_state(aTHX);

void
import(char *cl)
  PPCODE:
    /* Currently disabled lexicalization hacks/experiments */
    /*
    const PERL_CONTEXT *cxt = caller_cx(1, NULL);
    cophh_store_pvs(cxt->cx_u.cx_blk.blku_oldcop->cop_hints_hash, "jit", sv_2mortal(newSViv(1)), 0);
    */
    XSRETURN_EMPTY;
