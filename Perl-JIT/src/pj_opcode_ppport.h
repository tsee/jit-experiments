#ifndef PJ_OPCODE_PPPORT_H_
#define PJ_OPCODE_PPPORT_H_

// Should never matter, just to get the "permissible OPs" list to be
// insensitive to not knowing OP_AELEMFAST_LEX on older perls.
// (Same comment applies to the other forward/backward checks below)

#if PERL_VERSION <= 14
# define OP_AELEMFAST_LEX -1
#endif

#if PERL_VERSION <= 16
# define OP_FC -1
# define OP_LEAVEGIVEN -1
# define OP_ENTERGIVEN -1
# define OP_LEAVEWHEN -1
# define OP_ENTERWHEN -1
#endif

// OP_BOOLKEYS is history in 5.18
#if PERL_VERSION >= 17
# define OP_BOOLKEYS -1
#endif

#endif
