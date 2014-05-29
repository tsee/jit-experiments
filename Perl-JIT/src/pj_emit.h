#ifndef PJ_EMIT_H_
#define PJ_EMIT_H_

#include "EXTERN.h"
#include "perl.h"

namespace PerlJIT {
  void pj_init_emitter(pTHX);

  void pj_jit_sub(SV *coderef);
}

#endif
