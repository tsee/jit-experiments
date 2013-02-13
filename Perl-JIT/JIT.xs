#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"

#include <assert.h>

static XOP my_xop_addop;
static OP *my_pp_add(pTHX);

/* original peephole optimizer */
static peep_t orig_peepp;

static void
attempt_add_jit(pTHX_ BINOP *addop)
{
  OP *left  = addop->op_first;
  OP *right = addop->op_last;

  printf("left input: %s (%s)\n", OP_NAME(left), OP_DESC(left));
  printf("right input: %s (%s)\n", OP_NAME(right), OP_DESC(right));

}

static void my_peep(pTHX_ OP *o)
{
  const OP const *root = o;
  /* SV *hint; */

  orig_peepp(aTHX_ o);

  printf("Looking at: %s (%s)\n", OP_NAME(o), OP_DESC(o));
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


#define TOVISIT_PUSH(x) STMT_START { tovisit[iop++] = (x); } STMT_END
#define TOVISIT_POP tovisit[--iop]
#define TOVISIT_DONE iop == 0
  /* FIXME lazy, use a static-size stack for now. This will segfault. */
  OP *tovisit[1024];
  unsigned int iop = 0;

  OP *kid = o;
  while (kid->op_sibling != NULL) {
    kid = kid->op_sibling;
    TOVISIT_PUSH(kid);
  }

  while (!TOVISIT_DONE) {
    o = TOVISIT_POP;

    printf("Walking: Looking at: %s (%s)\n", OP_NAME(o), OP_DESC(o));

    if (o->op_type == OP_ADD) {
      printf("Found candidate!\n");
      attempt_add_jit(aTHX_ cBINOPo);
    }
    else { /* only recurse if not a candidate - for now */

      /* TODO s/// can have more stuff inside (can steal from B::Concise later) */
      if (o->op_flags & OPf_KIDS) {
        printf("Who knew? %s has kids.\n", OP_NAME(o));
        for (kid = cBINOPo->op_first; kid != NULL; kid = kid->op_sibling) {
          TOVISIT_PUSH(kid);
        }
      }
    }
  }
}


OP *
my_pp_add(pTHX)
{
  return NULL;
}

MODULE = Perl::JIT	PACKAGE = Perl::JIT

BOOT:
  /* setup our new peephole optimizer */
  orig_peepp = PL_peepp;
  PL_peepp = my_peep;
  /* setup our custom op */
  XopENTRY_set(&my_xop_addop, xop_name, "jitaddop");
  XopENTRY_set(&my_xop_addop, xop_desc, "An addop that makes useless use of JIT");
  XopENTRY_set(&my_xop_addop, xop_class, OA_BINOP);
  Perl_custom_op_register(aTHX_ my_pp_add, &my_xop_addop);

void
import(char *cl)
  PPCODE:
    /* Currently disabled lexicalization hacks/experiments */
    /*
    const PERL_CONTEXT *cxt = caller_cx(1, NULL);
    cophh_store_pvs(cxt->cx_u.cx_blk.blku_oldcop->cop_hints_hash, "jit", sv_2mortal(newSViv(1)), 0);
    */
    XSRETURN_EMPTY;
