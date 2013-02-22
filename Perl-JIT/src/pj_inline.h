#ifndef PJ_INLINE_H_
#define PJ_INLINE_H_

/* Setup aliases for "static inline" for portability. */

#include <perl.h>

/* Alias Perl's */
#define PJ_STATIC STATIC

#if defined(NDEBUG)
#  if defined(_MSC_VER)
#    define PJ_STATIC_INLINE STATIC __inline
#  else
#    define PJ_STATIC_INLINE STATIC inline
#  endif
#else
   /* avoid inlining under debugging */
#  define PJ_STATIC_INLINE STATIC
#endif

#endif
