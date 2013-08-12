#ifndef PJ_GLOBAL_STATE_H_
#define PJ_GLOBAL_STATE_H_

/* Global state of the JIT optimizer and
 * global init/cleanup logic. */

/* The less there is in here, the better. */

#include <EXTERN.h>
#include <perl.h>
#include <jit/jit.h>

/* The custom op definition structure */
/* extern XOP PJ_xop_jitop; */

/* Original opfreehook - we wrap this to free JIT OP aux structs */
/* extern Perl_ophook_t PJ_orig_opfreehook; */

/* Global state. Obviously not thread-safe.
 * Thread-safety would require this to be dangling off the
 * interpreter struct in some fashion. */
/* extern jit_context_t PJ_jit_context; */

/* Initialize global JIT state like JIT context, custom op description, etc. */
void pj_init_global_state(pTHX);

/* export constants to Perl */
void pj_define_constants(pTHX);

#endif
