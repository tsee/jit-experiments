#include "pj_global_state.h"
#include "pj_debug.h"

XOP PJ_xop_jitop;

peep_t PJ_orig_peepp;

Perl_ophook_t PJ_orig_opfreehook;

jit_context_t PJ_jit_context = NULL; /* jit_context_t is a ptr */
