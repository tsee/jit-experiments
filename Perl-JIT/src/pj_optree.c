#include "pj_optree.h"
#include <stdlib.h>
#include <stdio.h>

#include "ppport.h"
#include "pj_debug.h"
#include "stack.h"
#include "pj_ast_terms.h"
#include "pj_jit_op.h"

#define IS_JITTABLE_ROOT_OP_TYPE(otype) \
        (otype == OP_ADD || otype == OP_SUBTRACT)

#define IS_JITTABLE_OP_TYPE(otype) \
        (IS_JITTABLE_ROOT_OP_TYPE(otype) \
          || otype == OP_PADSV \
          || otype == OP_CONST)

/* Walk OP tree recursively, build ASTs, build subtrees */
static pj_term_t *
pj_build_ast(pTHX_ OP *o, ptrstack_t **subtrees, unsigned int *nvariables)
{
  const unsigned int parent_otype = o->op_type;
  pj_term_t *retval = NULL;
  OP *kid;

  /* 2 is the maximum number of children that the supported
   * OP types may have. Will change in future */
  pj_term_t *kid_terms[2];
  unsigned int ikid = 0;
  unsigned int i;

  PJ_DEBUG_2("pj_build_ast running on %s. Have %i subtrees right now.\n", OP_NAME(o), (int)(ptrstack_nelems(*subtrees)));

  if (o && (o->op_flags & OPf_KIDS)) {
    for (kid = ((UNOP*)o)->op_first; kid; kid = kid->op_sibling) {
      PJ_DEBUG_2("pj_build_ast considering kid (%u) type %s\n", ikid, OP_NAME(kid));

      const unsigned int otype = kid->op_type;
      if (otype == OP_CONST) {
        PJ_DEBUG("CONST being inlined.\n");
        kid_terms[ikid] = pj_make_const_dbl(SvNV(cSVOPx_sv(kid))); /* FIXME replace type by inferred type */
      }
      else if (otype == OP_PADSV) {
        kid_terms[ikid] = pj_make_variable((*nvariables)++, pj_double_type); /* FIXME replace pj_double_type with type that's imposed by the current OP */
        PJ_DEBUG("PADSV being added to subtrees.\n");
        ptrstack_push(*subtrees, pj_double_type); /* FIXME replace pj_double_type with type that's imposed by the current OP */
        ptrstack_push(*subtrees, kid);
      }
      else if (IS_JITTABLE_OP_TYPE(otype)) {
        kid_terms[ikid] = pj_build_ast(aTHX_ kid, subtrees, nvariables);
        if (kid_terms[ikid] == NULL) {
          for (i = 0; i < ikid; ++i)
            pj_free_tree(kid_terms[ikid]);
          return NULL;
        }
      }
      else {
        /* Can't represent OP with AST. So instead,
         * recursively scan for separate candidates and
         * treat as subtree. */
        PJ_DEBUG_1("Cannot represent this OP with AST. Emitting variable. (%s)", OP_NAME(kid));
        pj_find_jit_candidate(aTHX_ kid, o); /* o is parent of kid */
        kid_terms[ikid] = pj_make_variable((*nvariables)++, pj_double_type); /* FIXME replace pj_double_type with type that's imposed by the current OP */

        ptrstack_push(*subtrees, pj_double_type); /* FIXME replace pj_double_type with type that's imposed by the current OP */
        ptrstack_push(*subtrees, kid);
      }

      ++ikid;
    } /* end for kids */

    /* FIXME find a way of doing this that is less manual/verbose */
    if (parent_otype == OP_ADD) {
      assert(ikid == 2);
      retval = pj_make_binop(
        pj_binop_add,
        kid_terms[0],
        kid_terms[1]
      );
    }
    else if (parent_otype == OP_SUBTRACT) {
      assert(ikid == 2);
      retval = pj_make_binop(
        pj_binop_subtract,
        kid_terms[0],
        kid_terms[1]
      );
    }
    else {
      PJ_DEBUG_1("Shouldn't happen! Unsupported OP!? %s", OP_NAME(o));
      abort();
    }

  } /* end if has kids */
  else { /* OP without kids */
    /* OP_PADSV and OP_CONST are handled in the caller as other OPs' kids */
    PJ_DEBUG_1("ARG! Unsupported OP without kids, %s", OP_NAME(o));
    abort();
  }

  /* PMOP doesn't matter for JIT right now */
  /*
    if (o && OP_CLASS(o) == OA_PMOP && o->op_type != OP_PUSHRE
          && (kid = PMOP_pmreplroot(cPMOPo)))
    {}
  */

  PJ_DEBUG_1("Returning from pj_build_ast. Have %i subtrees right now.\n", (int)(ptrstack_nelems(*subtrees)/2));
  return retval;
}

static void
pj_build_jitop_kid_list(pTHX_ LISTOP *jitop, ptrstack_t *subtrees)
{
  if (ptrstack_empty(subtrees)) {
    jitop->op_first = NULL; /* FIXME is this valid for a LISTOP? */
    jitop->op_last = NULL;
  }
  else {
    void **subtree_array = ptrstack_data_pointer(subtrees);
    const unsigned int n = ptrstack_nelems(subtrees);
    unsigned int i;
    OP *o = NULL;

    /* TODO for now, we just always impose "numeric". Later, this may need
     *      to be flexible. */
    o = (OP *)subtree_array[1];
    jitop->op_first = o;
    PJ_DEBUG_1("First kid is %s\n", OP_NAME(o));

    /* Alternating op-imposed-type and actual subtree */
    for (i = 2; i < n; i += 2) {
      PJ_DEBUG_2("Kid %u is %s\n", (int)(i/2)+1, OP_NAME(o));
      /* TODO get the imposed type context from subtree_array[i] here */
      o->op_sibling = subtree_array[i+1];
      o = o->op_sibling;
    }
    jitop->op_last = o;
    o->op_sibling = NULL;
  }
}


static void
pj_fixup_parent_op(pTHX_ OP *jitop, OP *origop, UNOP *parentop)
{
  OP *kid;

  if (parentop->op_first == origop) {
    parentop->op_first = jitop;
  }

  for (kid = parentop->op_first; kid; kid = kid->op_sibling) {
    if (kid->op_sibling && kid->op_sibling == origop) {
      kid->op_sibling = jitop;
      break;
    }
  }

  if (OP_CLASS((OP *)parentop) != OA_UNOP) {
    if (((BINOP *)parentop)->op_last == origop)
      ((BINOP *)parentop)->op_last = jitop;
  }

  jitop->op_sibling = origop->op_sibling;
}


/* Starting from a candidate for JITing, walk the OP tree to accumulate
 * a subtree that can be replaced with a single JIT OP. */
/* TODO: Needs to walk the OPs, checking whether they qualify. If
 *       not, then that subtree needs to be added to the list of
 *       trees to be executed before executing the JIT OP itself,
 *       so that their return values end up on the stack
 *       (warning: TARG optimizations!). Also needs to record the
 *       kind of OP that includes the unJITable subtree so that
 *       "type context" can be inferred. Needs recurse depth-first,
 *       left-hugging in order to get the sub tree is normal
 *       execution order. */
static void
pj_attempt_jit(pTHX_ OP *o, OP *parentop)
{
  /* In reality, we don't use the ptrstack_t as a proper stack,
   * but more of a dynamically growing array */
  ptrstack_t *subtrees;
  pj_term_t *ast;
  unsigned int nvariables = 0;

  PJ_DEBUG_1("Attempting JIT on %s\n", OP_NAME(o));
  subtrees = ptrstack_make(3, 0);

  ast = pj_build_ast(aTHX_ o, &subtrees, &nvariables);

  if (ast != NULL) {
    OP *jitop;
    pj_jitop_aux_t *jitop_aux;

    PJ_DEBUG_2("Built actual AST for jitting. Have %i subtrees which means %i variables.\n", (int)(ptrstack_nelems(subtrees)/2), nvariables);
    if (PJ_DEBUGGING)
      pj_dump_tree(ast);

    jitop = (OP *)pj_prepare_jit_op(aTHX_ nvariables, o);
    PJ_DEBUG_1("Have a JIT OP: %s\n", OP_NAME(jitop));

    /* The following function call will build the usual LISTOP
     * structure where op_first points at the start of the linked
     * list of kids and op_last points at the end. The kids
     * are linked using their op_sibling pointer.
     *
     *       /-----JITOP------\
     *      /                  \
     *     /op_first            \op_last
     *    /                      \
     *   OP ---> OP ---> ... ---> OP
     *      op_s    op_s     op_s
     *
     * where op_s is understood to be "op_sibling".
     */
    pj_build_jitop_kid_list(aTHX_ (LISTOP *)jitop, subtrees);

    pj_fixup_parent_op(aTHX_ jitop, o, (UNOP *)parentop);

    /* TODO clean up orphaned OPs */

    jitop_aux = (pj_jitop_aux_t *)jitop->op_targ;
    /* TODO JIT IT FOR REAL */
  }

  pj_free_tree(ast);
  ptrstack_free(subtrees);
}

/* inspired by B.xs */
#define PMOP_pmreplstart(o)	o->op_pmstashstartu.op_pmreplstart
#define PMOP_pmreplroot(o)	o->op_pmreplrootu.op_pmreplroot

/* Traverse OP tree from o until done OR a candidate for JITing was found.
 * For candidates, invoke JIT attempt and then move on without going into
 * the particular sub-tree. */
void
pj_find_jit_candidate(pTHX_ OP *o, OP *parentop)
{
  unsigned int otype;
  OP *kid;
  ptrstack_t *backlog;

  backlog = ptrstack_make(8, 0);
  ptrstack_push(backlog, parentop);
  ptrstack_push(backlog, o);

  /* Iterative tree traversal using stack */
  while (!ptrstack_empty(backlog)) {
    o = ptrstack_pop(backlog);
    parentop = ptrstack_pop(backlog);
    otype = o->op_type;

    PJ_DEBUG_1("Considering %s\n", OP_NAME(o));

    /* Attempt JIT if the right OP type. Don't recurse if so. */
    if (IS_JITTABLE_ROOT_OP_TYPE(otype)) {
      if (parentop != NULL) {
        /* Can only JIT if we have the parent OP. Some time later, maybe
         * I'll discover a way to find the parent... */
        pj_attempt_jit(aTHX_ o, parentop);
      }
      else
        PJ_DEBUG_1("Might have been able to JIT %s, but parent OP is NULL", OP_NAME(o));
    }
    else {
      if (o && (o->op_flags & OPf_KIDS)) {
        for (kid = ((UNOP*)o)->op_first; kid; kid = kid->op_sibling) {
          ptrstack_push(backlog, o); /* parent for kid */
          ptrstack_push(backlog, kid);
        }
      }

      if (o && OP_CLASS(o) == OA_PMOP && o->op_type != OP_PUSHRE
            && (kid = PMOP_pmreplroot(cPMOPo)))
      {
        ptrstack_push(backlog, o); /* parent for kid */
        ptrstack_push(backlog, kid);
      }
    } /* end "not a jittable root OP" */
  } /* end while stuff on todo stack */

  ptrstack_free(backlog);
}
