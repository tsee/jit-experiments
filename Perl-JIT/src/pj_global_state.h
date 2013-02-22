#ifndef PJ_GLOBAL_STATE_H_
#define PJ_GLOBAL_STATE_H_

#include <EXTERN.h>
#include <perl.h>
#include <jit/jit.h>

/* The custom op definition structure */
extern XOP PJ_xop_jitop;

/* Original peephole optimizer */
extern peep_t PJ_orig_peepp;

/* Original opfreehook - we wrap this to free JIT OP aux structs */
extern Perl_ophook_t PJ_orig_opfreehook;

/* Global state. Obviously not thread-safe.
 * Thread-safety would require this to be dangling off the
 * interpreter struct in some fashion. */
extern jit_context_t PJ_jit_context;



#endif
