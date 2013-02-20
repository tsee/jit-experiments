#ifndef DYNSTACK_H_
#define DYNSTACK_H_

#include <stdlib.h>

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

static inline ptrstack_t *
ptrstack_make(const unsigned int initsize, unsigned int flags_)
{
  ptrstack_t *stack = (ptrstack_t *)malloc(sizeof(ptrstack_t));
  if (stack == NULL)
    return NULL;

  stack->size = (initsize == 0 ? 16 : initsize);
  stack->nextpos = 0;
  stack->flags = flags_;
  stack->data = malloc(sizeof(void *) * stack->size);

  return stack;
}

static inline void
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

static int
ptrstack_grow(ptrstack_t *stack, unsigned int nelems)
{
  const unsigned int newsize
    = (unsigned int)( ( ( stack->size > nelems
                          ? stack->size
                          : nelems) + 5 /* some static growth for avoiding lots
                                         * of reallocs on very small stacks */
                      ) * PTRSTACK_GROWTH_FACTOR);
  stack->data = realloc(stack->data, sizeof(void *) * newsize);
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
static inline int
ptrstack_assert_space(ptrstack_t *stack, const unsigned int nelems)
{
  return PTRSTACK_SPACE_ASSERT(stack, nelems);
}

static inline int
ptrstack_push(ptrstack_t *stack, void *elem)
{
  if (PTRSTACK_SPACE_ASSERT(stack, 1))
    return 1; /* OOM */
  stack->data[stack->nextpos++] = elem;
  return 0;
}

#undef PTRSTACK_SIZE_ASSERT

static inline int
ptrstack_push_nocheck(ptrstack_t *stack, void *elem)
{
  stack->data[stack->nextpos++] = elem;
  return 0;
}

static inline void *
ptrstack_pop(ptrstack_t *stack)
{
  return stack->nextpos == 0
         ? NULL
         : stack->data[--stack->nextpos];
}

static inline void *
ptrstack_peek(ptrstack_t *stack)
{
  return stack->nextpos == 0
         ? NULL
         : stack->data[stack->nextpos - 1];
}

/* If you're done pushing and the stack is there to stay,
 * you can compact. Not usually very useful, though. */
static inline void
ptrstack_compact(ptrstack_t *stack)
{
  const unsigned int minsize = stack->nextpos + 1;
  if (stack->size > minsize) {
    stack->data = realloc(stack->data, sizeof(void *) * minsize);
    stack->size = minsize;
  }
}

static inline unsigned int
ptrstack_nelems(ptrstack_t *stack)
{
  return stack->nextpos;
}

static inline int
ptrstack_empty(ptrstack_t *stack)
{
  return (stack->nextpos == 0);
}

static inline void *
ptrstack_data_pointer(ptrstack_t *stack)
{
  return *stack->data;
}


#endif
