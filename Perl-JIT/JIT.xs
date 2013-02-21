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
#include "pj_terms.h"
#include "pj_jit.h"
#include "pj_optree.h"
#include "pj_debug.h"

/* The struct of pertinent per-OP instance
 * data that we attach to each JIT OP. */
typedef struct {
  void (*jit_fun)(void);
  NV *paramslist;
  UV nparams;
  PADOFFSET saved_op_targ; /* Replacement for JIT OP's op_targ if necessary */
} pj_jitop_aux_t;

/* The actual custom op definition structure */
static XOP my_xop_jitop;
/* The generic custom OP implementation - push/pop function */
static OP *my_pp_jit(pTHX);
/* Original peephole optimizer */
static peep_t orig_peepp;

/* Original opfreehook - we wrap this to free JIT OP aux structs */
static Perl_ophook_t orig_opfreehook;

/* Global state. Obviously not thread-safe.
 * Thread-safety would require this to be dangling off the
 * interpreter struct in some fashion. */
static jit_context_t pj_jit_context = NULL; /* jit_context_t is a ptr */

/* End-of-global-destruction cleanup hook.
 * Actually installed in BOOT XS section. */
void
pj_jit_final_cleanup(pTHX_ void *ptr)
{
  (void)ptr;
  
  PJ_DEBUG("pj_jit_final_cleanup after global destruction.\n");

  if (pj_jit_context == NULL)
    jit_context_destroy(pj_jit_context);
}

/* Hook that will free the JIT OP aux structure of our custom ops */
/* FIXME this doesn't appear to actually be called for all ops -
 *       specifically NOT for our custom OP. Is this because the
 *       custom OP isn't wired up correctly? */
static void
my_opfreehook(pTHX_ OP *o)
{
  if (orig_opfreehook != NULL)
    orig_opfreehook(aTHX_ o);

  /* printf("cleaning %s\n", OP_NAME(o)); */
  if (o->op_ppaddr == my_pp_jit) {
    PJ_DEBUG("Cleaning up custom OP's pj_jitop_aux_t\n");
    pj_jitop_aux_t *aux = (pj_jitop_aux_t *)o->op_targ;
    free(aux->paramslist);
    free(aux);
    o->op_targ = 0; /* important or Perl will use it to access the pad */
  }
}


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

  if (0 == pj_tree_jit(pj_jit_context, jit_ast, &func, &funtype)) {
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
  jitop->op_ppaddr = my_pp_jit;

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

static void jit_peep(pTHX_ OP *o)
{
  OP *root = o;
  /* SV *hint; */

  pj_find_jit_candidate(aTHX_ root);

  return; /* Do not run old JIT code any more: Being reworked! */

  PJ_DEBUG_2("Looking at: %s (%s)\n", OP_NAME(o), OP_DESC(o));
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

  ptrstack_t *tovisit;
  tovisit = ptrstack_make(20, 0);

  OP *kid = o;
  while (kid->op_sibling != NULL) {
    kid = kid->op_sibling;
    ptrstack_push(tovisit, root);
    ptrstack_push(tovisit, kid);
  }

  OP *parent = root;
  while (!ptrstack_empty(tovisit)) {
    int do_recurse = 1;
    o = ptrstack_pop(tovisit);
    parent = ptrstack_pop(tovisit);

    if (PJ_DEBUGGING)
      printf("Walking: Looking at: %s (%s); parent: %s\n", OP_NAME(o), OP_DESC(o), OP_NAME(parent));

    if (o->op_type == OP_ADD)
    {
      if (   (cBINOPo->op_first->op_type == OP_PADSV || cBINOPo->op_first->op_type == OP_CONST)
          && (cBINOPo->op_last->op_type == OP_PADSV || cBINOPo->op_last->op_type == OP_CONST) )
      {
        PJ_DEBUG_1("Found candidate with parent of type %s!\n", OP_NAME(parent));
        attempt_add_jit_proof_of_principle(aTHX_ cBINOPo, parent);
        do_recurse = 0; /* for now */
      }
    }

    /* TODO s/// can have more stuff inside but not OPf_KIDS set? (can steal from B::Concise later) */
    if (do_recurse && o->op_flags & OPf_KIDS) {
      PJ_DEBUG_1("Who knew? %s has kids.\n", OP_NAME(o));
      for (kid = cBINOPo->op_first; kid != NULL; kid = kid->op_sibling) {
        ptrstack_push(tovisit, o);
        ptrstack_push(tovisit, kid);
      }
    }
  }

  ptrstack_free(tovisit);

  orig_peepp(aTHX_ root);
}


OP *
my_pp_jit(pTHX)
{
  dVAR; dSP;
  /* Traditionally, addop uses dATARGET here, but that relies
   * on being able to use PL_op->op_targ as a PAD offset sometimes.
   * For the JIT OP, this info comes from the aux struct, so we need
   * to inline a modified version of dATARGET. */
  SV *targ;

  pj_jitop_aux_t *aux = (pj_jitop_aux_t *) ((BINOP *)PL_op)->op_targ;

  SV *tmpsv;
  unsigned int i, n;

  //printf("Custom op called\n");

  /* inlined modified dATARGET, see above */
  targ = PL_op->op_flags & OPf_STACKED
         ? sp[-1]
         : PAD_SV(aux->saved_op_targ);

  /* This implements overload and other magic horribleness */
  tryAMAGICbin_MG(add_amg, AMGf_assign|AMGf_numeric);

  {
    double result; /* FIXME function ret type should be dynamic */
    double *params = aux->paramslist;
    n = aux->nparams;

    /* Pop all args from stack except the last */
    for (i = 0; i < n-1; ++i) {
      tmpsv = POPs;
      params[i] = SvNV_nomg(tmpsv);
    }
    /* If there's any args, leave the final one's SV on the stack */
    if (n != 0) {
      tmpsv = TOPs;
      params[n-1] = SvNV_nomg(tmpsv);
    }
    //printf("In: %f %f\n", params[0], params[1]);
    pj_invoke_func((pj_invoke_func_t) aux->jit_fun, params, aux->nparams, pj_double_type, (void *)&result);

    PJ_DEBUG_1("Add result from JIT: %f\n", (float)result);
    SETn((NV)result);
  }

  RETURN;
}


MODULE = Perl::JIT	PACKAGE = Perl::JIT

BOOT:
  {
    /* Setup our new peephole optimizer */
    orig_peepp = PL_peepp;
    PL_peepp = jit_peep;

    /* Set up JIT compiler */
    pj_jit_context = jit_context_create();

    /* Setup our callback for cleaning up JIT OPs during global cleanup */
    orig_opfreehook = PL_opfreehook;
    PL_opfreehook = my_opfreehook;

    /* Setup our custom op */
    XopENTRY_set(&my_xop_jitop, xop_name, "jitop");
    XopENTRY_set(&my_xop_jitop, xop_desc, "a just-in-time compiled composite operation");
    XopENTRY_set(&my_xop_jitop, xop_class, OA_LISTOP);
    Perl_custom_op_register(aTHX_ my_pp_jit, &my_xop_jitop);

    /* Register super-late global cleanup hook for global JIT state */
    Perl_call_atexit(aTHX_ pj_jit_final_cleanup, NULL);
  }

void
import(char *cl)
  PPCODE:
    /* Currently disabled lexicalization hacks/experiments */
    /*
    const PERL_CONTEXT *cxt = caller_cx(1, NULL);
    cophh_store_pvs(cxt->cx_u.cx_blk.blku_oldcop->cop_hints_hash, "jit", sv_2mortal(newSViv(1)), 0);
    */
    XSRETURN_EMPTY;
