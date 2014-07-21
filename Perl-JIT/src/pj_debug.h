#ifndef PJ_DEBUG_H_
#define PJ_DEBUG_H_

/* Set up debugging output to be compiled out without assertions */

#ifndef DEBUG_OUTPUT
#  define PJ_DEBUGGING 0
#  define PJ_DEBUG(s) ((void)(s))
#  define PJ_DEBUG_1(s, par1) ((void)(s), (void)(par1))
#  define PJ_DEBUG_2(s, par1, par2) ((void)(s), (void)(par1), (void)(par2))
#  define PJ_DEBUG_3(s, par1, par2, par3) ((void)(s), (void)(par1), (void)(par2), (void)(par3))
#else
#  define PJ_DEBUGGING 1
#  define PJ_DEBUG(s) do { printf(s); fflush(stdout); } while(0)
#  define PJ_DEBUG_1(s, par1) do { printf(s, par1); fflush(stdout); } while(0)
#  define PJ_DEBUG_2(s, par1, par2) do { printf(s, par1, par2); fflush(stdout); } while(0)
#  define PJ_DEBUG_3(s, par1, par2, par3) do { printf(s, par1, par2, par3); fflush(stdout); } while(0)
#endif

#endif
