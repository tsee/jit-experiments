#ifndef PJ_OPTREE_H_
#define PJ_OPTREE_H_

/* Starting from root OP, traverse the tree to find candidate OP for JITing
 * and perform actual replacement if at all. */
/* This function will internally call pj_op_tree_to_pj_ast on candidates,
 * which will, in turn, call this function on subtrees that it cannot
 * JIT.
 * FIXME: limit depth of recursion of write iterative version with
 *        stack instead. pj_find_jit_candidate should be relatively easy
 *        to do with a stack (see stack.h in same directory!). */
void pj_find_jit_candidate(pTHX_ OP *o);

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
/* pj_op_tree_to_pj_ast(pTHX_ OP *o, ...) */

#endif
