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


static void
fixup_parent_op(pTHX_ OP *parent, OP *oldkid, OP *newkid)
{
  OP *kid;
  /* fixup parent's basic order ptr */
  printf("Parent op is a: %s (class %i. OA_LIST==%i)\n", OP_NAME(parent), OP_CLASS(parent), OA_LISTOP);

  if (((BINOP *)parent)->op_first == (OP *)oldkid) {
    ((BINOP *)parent)->op_first = newkid;
    PJ_DEBUG("Replaced parent pointer in op_first!\n");
    return;
  }

  if (OP_CLASS(parent) == OA_LISTOP) {
    kid = ((BINOP *)parent)->op_first;
    for (; kid != ((BINOP *)parent)->op_last && kid != NULL; kid = kid->op_sibling)
    {
      if (kid->op_sibling == (OP *)oldkid) {
        ((BINOP *)kid)->op_sibling = newkid;
        PJ_DEBUG("Replaced parent pointer in op_first => op_last list!\n");
        return;
      }
    }
  }

  if (OP_CLASS(parent) == OA_COP) {
    OP *lastop = ((BINOP *)parent)->op_last;
    for (; parent != lastop && parent != NULL; parent = parent->op_sibling) {
      if (parent->op_sibling == (OP *)oldkid) {
        ((BINOP *)parent)->op_sibling = newkid;
        PJ_DEBUG("Replaced parent pointer in sibling list!\n");
        return;
      }
    }
  }

  PJ_DEBUG("Failed to find pointer from parent op to op-to-be-replaced.\n");
  abort();
}

static void
attempt_add_jit_proof_of_principle(pTHX_ BINOP *addop, OP *parent)
{
  LISTOP *jitop;
  OP *left  = addop->op_first;
  OP *right = addop->op_last;
  pj_jitop_aux_t *jit_aux;
  pj_term_t *jit_ast;
  jit_function_t func = NULL;
  pj_basic_type funtype;
  unsigned int iparam = 0;

  PJ_DEBUG_2("left input: %s (%s)\n", OP_NAME(left), OP_DESC(left));

  /* Create a custom op! */
  NewOp(1101, jitop, 1, LISTOP);
  jitop->op_type = (OPCODE)OP_CUSTOM;
  jitop->op_first = left;
  left->op_sibling = right; /* It's a LISTOP, kids must be linked list */
  jitop->op_last = right;
  jitop->op_private = 0;
  /* Commonly op_flags is:
   *   OP receives some args via the stack and has kids (OPf_STACKED | OPf_KIDS).
   * but that's not always true. For the time being, copying from addop is good enough.
   * That needs proper understanding once we replace entire subtrees. */
  jitop->op_flags = addop->op_flags;

  if (addop->op_private & OPpTARGET_MY) {
    /* If this flag is set on the original OP, then we have a nasty situation.
     * In a nutshell, this is set as an optimization for scalar assignment
     * to a pad (== lexical) variable. If set, the addop will directly
     * assign to whichever pad variable would otherwise be set by the sassign
     * op. It won't bother putting a separate var on the stack.
     * This is great, but it uses the op_targ member of the OP struct to
     * define the offset into the pad where the output variable is to be found.
     * That's a problem because we're using op_targ to hang the jit aux struct
     * off of.
     */
    /* printf("addop TARG is used. Going to SEGV now.\n"); */
    jitop->op_private |= OPpTARGET_MY;
  }

  /* Attach JIT info to it */
  jit_aux = malloc(sizeof(pj_jitop_aux_t));
  jit_aux->paramslist = (NV *)malloc(sizeof(NV) * 2); /* FIXME hardcoded to double params for now */
  jit_aux->jit_fun = NULL;
  jit_aux->saved_op_targ = addop->op_targ; /* save in case needed for sassign optimization */

  /* It may turn out that op_targ is not safe to use for custom OPs because
   * some core functions may meddle with it. But chances are it's fine. */
  jitop->op_targ = (PADOFFSET)PTR2UV(jit_aux);

  /* JIT IT! */
  /* Right now, we special-case-support $x+$y or $x+const or const+$y */
  jit_ast = pj_make_binop(
    pj_binop_add,
    ( left->op_type == OP_CONST
      ? pj_make_const_dbl( SvNV_nomg(cSVOPx_sv(left)) )
      : pj_make_variable(iparam++, pj_double_type) ),
    ( right->op_type == OP_CONST
      ? pj_make_const_dbl( SvNV_nomg(cSVOPx_sv(right)) )
      : pj_make_variable(iparam++, pj_double_type) )
  );
  pj_dump_tree(jit_ast);

  jit_aux->nparams = iparam;

  if (0 == pj_tree_jit(PJ_jit_context, jit_ast, &func, &funtype)) {
    PJ_DEBUG("JIT succeeded!\n");
  } else {
    PJ_DEBUG("JIT failed!\n");
  }
  void *cl = jit_function_to_closure(func);
  jit_aux->jit_fun = cl;

  /* Release JIT AST after being done with compilation */
  free(jit_ast);
  jit_ast = NULL;

  /* Set it's implementation ptr */
  jitop->op_ppaddr = pj_pp_jit;

  /* Expected output execution order for two PADSV ops:
   * ---> left -> right -> jitop ---> */

  /* FIXME this doesn't support compiling out the left OP yet because
   * whichever incoming op_next ptr goes to left can't easily be repointed.
   * This may need building a backwards-directed linked list? Yuck. */
  /* Wire pointer to -> jitop */
  if (right->op_type == OP_CONST) {
    /* Remove right op from execution thread */
    left->op_next = (OP *)jitop;
    Perl_op_null(aTHX_ right); /* just because */
  }
  else {
    /* already the default: left->op_next = (OP *)right; */
    right->op_next = (OP *)jitop;
  }
  /* Best we can do right now for left op is nulling */
  if (left->op_type == OP_CONST) {
    Perl_op_null(aTHX_ left);
  }

  /* wire jitop -> old pp_next */
  jitop->op_next = addop->op_next;

  /* Expected final basic structure:
   *  L   R
   *   \ /
   *    J
   *     \
   *      P
   */

  /* Fixup new OP's sibling ptr to fall into place of the old one (if any) */
  jitop->op_sibling = addop->op_sibling;

  /* fixup parent's basic order ptr */
  fixup_parent_op(aTHX_ (OP *)parent, (OP *)addop, (OP *)jitop);

  /* FIXME something still refers to the addop, so this segfaults.
   *       Maybe the parent and op_next fixups don't work?
   *       This would explain why the custom OP free hook never reaches
   *       the JIT OP. But execution of JIT OP seems to work just fine.
   *       That suggests that either I'm missing something fundamental, or the
   *       parent fixup is the part that's not right? */
  /* Perl_op_free(aTHX_ (OP *)addop); */
  /* for now, null the OP instead */
  Perl_op_null(aTHX_ (OP *)addop);
}

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
