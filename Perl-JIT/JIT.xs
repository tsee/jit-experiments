#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

static XOP my_xop;
static OP *my_pp(pTHX);

/* original per-op peephole optimizer */
static peep_t orig_rpeepp;

static void my_rpeep(pTHX_ OP *o)
{
  /* SV *hint; */

  orig_rpeepp(aTHX_ o);

  /* Currently disabled lexicalization hacks/experiments */
  /*
  hint = cop_hints_fetch_pvs(PL_curcop, "jit", 0);
  if (hint != NULL && SvTRUE(hint)) {
    printf("Optimizing this op!\n");
  }
  else {
    printf("NOT optimizing this op!\n");
  }
  */
}


OP *
my_pp(pTHX)
{
  return NULL;
}

MODULE = Perl::JIT	PACKAGE = Perl::JIT

BOOT:
  /* setup our new peephole optimizer */
  orig_rpeepp = PL_rpeepp;
  PL_rpeepp = my_rpeep;
  /* setup our custom op */
  XopENTRY_set(&my_xop, xop_name, "myxop");
  XopENTRY_set(&my_xop, xop_desc, "Useless custom op");
  XopENTRY_set(&my_xop, xop_class, OA_LISTOP);
  Perl_custom_op_register(aTHX_ my_pp, &my_xop);

void
import(char *cl)
  PPCODE:
    /* Currently disabled lexicalization hacks/experiments */
    /*
    const PERL_CONTEXT *cxt = caller_cx(1, NULL);
    cophh_store_pvs(cxt->cx_u.cx_blk.blku_oldcop->cop_hints_hash, "jit", sv_2mortal(newSViv(1)), 0);
    */
    XSRETURN_EMPTY;
