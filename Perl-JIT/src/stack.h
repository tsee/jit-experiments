#ifndef DYNSTACK_H_
#define DYNSTACK_H_

#include <stdlib.h>
#include <assert.h>
#include "pj_inline.h"

/* A simple, geometrically growing stack implementation using
 * void * as value type. All API functions are static, many inline. */

/* If set in the flags, this will cause remaining elements 
 * on the stack to be freed when ptrstack_free() is called. */
#define PTRSTACKf_FREE_ELEMS 1

/* Geometric growth of the stack */
#ifndef PTRSTACK_GROWTH_FACTOR
# define PTRSTACK_GROWTH_FACTOR 1.3
#endif

typedef struct {
  void **data;
  unsigned int size;
  unsigned int nextpos;
  unsigned int flags;
} ptrstack_t;

/****************************
 * API
 */

/* Constructor. If flags has PTRSTACKf_FREE_ELEMS set, then ptrstack_free will free()
 * all elements still on the stack. */
PJ_STATIC_INLINE ptrstack_t *ptrstack_make(const unsigned int initsize, unsigned int flags_);

/* Release ptrstack's memory. */
PJ_STATIC_INLINE void ptrstack_free(ptrstack_t *stack);

/* Add another element to the top of the stack. */
PJ_STATIC_INLINE int ptrstack_push(ptrstack_t *stack, void *elem);

/* Add another element to the top of the stack without checking whether there's
 * enough space for it. Purely an optimization to avoid a branch. */
PJ_STATIC_INLINE int ptrstack_push_nocheck(ptrstack_t *stack, void *elem);

/* Remove the top element off the stack and return it. Returns NULL if
 * the stack is empty. */
PJ_STATIC_INLINE void *ptrstack_pop(ptrstack_t *stack);

/* Remove top element off the stack and return it. Doesn't bother checking
 * whether the stack still has data, so can result in nasty things if
 * that's not known from your usage context. */
PJ_STATIC_INLINE void * ptrstack_pop_nocheck(ptrstack_t *stack);

/* Return the top element without removing it. */
PJ_STATIC_INLINE void *ptrstack_peek(ptrstack_t *stack);

/* Number of elements on the stack. */
PJ_STATIC_INLINE unsigned int ptrstack_nelems(ptrstack_t *stack);

/* Is the stack empty? */
PJ_STATIC_INLINE int ptrstack_empty(ptrstack_t *stack);



/* Returns the raw pointer to the stack contents, base at 0. */
PJ_STATIC_INLINE void **ptrstack_data_pointer(ptrstack_t *stack);

/* Force the stack to grow to at least the given absolute number of elements.
 * Not necessary to call manually. The stack will grow dynamically. */
STATIC int ptrstack_grow(ptrstack_t *stack, unsigned int nelems);

/* If you're done pushing and the stack is there to stay,
 * you can compact. Not usually very useful, though. */
PJ_STATIC_INLINE void ptrstack_compact(ptrstack_t *stack);

/* Assert that the stack has at least nelems of space left */
PJ_STATIC_INLINE int ptrstack_assert_space(ptrstack_t *stack, const unsigned int nelems);


/***************************
 * Implementation
 */

PJ_STATIC_INLINE ptrstack_t *
ptrstack_make(const unsigned int initsize, unsigned int flags_)
{
  ptrstack_t *stack = (ptrstack_t *)malloc(sizeof(ptrstack_t));
  if (stack == NULL)
    return NULL;

  stack->size = (initsize == 0 ? 16 : initsize);
  stack->nextpos = 0;
  stack->flags = flags_;
  stack->data = (void **) malloc(sizeof(void *) * stack->size);

  return stack;
}

PJ_STATIC_INLINE void
ptrstack_free(ptrstack_t *stack)
{
  if (stack->flags & PTRSTACKf_FREE_ELEMS) {
    unsigned int i;
    const unsigned int n = stack->nextpos;
    for (i = 1; i <= n; ++i)
      free(stack->data[n-i]);
  }
  
  free(stack->data);
  free(stack);
}

PJ_STATIC int
ptrstack_grow(ptrstack_t *stack, unsigned int nelems)
{
  const unsigned int newsize
    = (unsigned int)( ( ( stack->size > nelems
                          ? stack->size
                          : nelems) + 5 /* some static growth for avoiding lots
                                         * of reallocs on very small stacks */
                      ) * PTRSTACK_GROWTH_FACTOR);
  stack->data = (void **) realloc(stack->data, sizeof(void *) * newsize);
  if (stack->data == NULL)
    return 1; /* OOM */
  stack->size = newsize;
  return 0;
}

/* local macro to force inline */
#define PTRSTACK_SPACE_ASSERT(s, nelems)            \
    ((s)->size < (s)->nextpos+(nelems)              \
      ? ptrstack_grow((s), (s)->nextpos + (nelems)) \
      : 0)

/* just for the API */
PJ_STATIC_INLINE int
ptrstack_assert_space(ptrstack_t *stack, const unsigned int nelems)
{
  return PTRSTACK_SPACE_ASSERT(stack, nelems);
}

PJ_STATIC_INLINE int
ptrstack_push(ptrstack_t *stack, void *elem)
{
  if (PTRSTACK_SPACE_ASSERT(stack, 1))
    return 1; /* OOM */
  stack->data[stack->nextpos++] = elem;
  return 0;
}

PJ_STATIC_INLINE int
ptrstack_push_nocheck(ptrstack_t *stack, void *elem)
{
  assert(PTRSTACK_SPACE_ASSERT(stack, 1));
  stack->data[stack->nextpos++] = elem;
  return 0;
}

#undef PTRSTACK_SIZE_ASSERT

PJ_STATIC_INLINE void *
ptrstack_pop(ptrstack_t *stack)
{
  return stack->nextpos == 0
         ? NULL
         : stack->data[--stack->nextpos];
}

PJ_STATIC_INLINE void *
ptrstack_pop_nocheck(ptrstack_t *stack)
{
  assert(stack->nextpos > 0);
  return stack->data[--stack->nextpos];
}


PJ_STATIC_INLINE void *
ptrstack_peek(ptrstack_t *stack)
{
  return stack->nextpos == 0
         ? NULL
         : stack->data[stack->nextpos - 1];
}

PJ_STATIC_INLINE unsigned int
ptrstack_nelems(ptrstack_t *stack)
{
  return stack->nextpos;
}

PJ_STATIC_INLINE int
ptrstack_empty(ptrstack_t *stack)
{
  return (stack->nextpos == 0);
}

PJ_STATIC_INLINE void **
ptrstack_data_pointer(ptrstack_t *stack)
{
  return stack->data;
}


/* If you're done pushing and the stack is there to stay,
 * you can compact. Not usually very useful, though. */
PJ_STATIC_INLINE void
ptrstack_compact(ptrstack_t *stack)
{
  const unsigned int minsize = stack->nextpos + 1;
  if (stack->size > minsize) {
    stack->data = (void **) realloc(stack->data, sizeof(void *) * minsize);
    stack->size = minsize;
  }
}

#endif
