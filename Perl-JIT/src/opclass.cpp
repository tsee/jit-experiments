#include "opclass.h"

typedef enum {
    OPc_NULL,   /* 0 */
    OPc_BASEOP, /* 1 */
    OPc_UNOP,   /* 2 */
    OPc_BINOP,  /* 3 */
    OPc_LOGOP,  /* 4 */
    OPc_LISTOP, /* 5 */
    OPc_PMOP,   /* 6 */
    OPc_SVOP,   /* 7 */
    OPc_PADOP,  /* 8 */
    OPc_PVOP,   /* 9 */
    OPc_LOOP,   /* 10 */
    OPc_COP     /* 11 */
} opclass;

static const char* const opclassnames[] = {
    "B::NULL",
    "B::OP",
    "B::UNOP",
    "B::BINOP",
    "B::LOGOP",
    "B::LISTOP",
    "B::PMOP",
    "B::SVOP",
    "B::PADOP",
    "B::PVOP",
    "B::LOOP",
    "B::COP"    
};

static opclass
cc_opclass(pTHX_ const OP *o)
{
    bool custom = 0;

    if (!o)
        return OPc_NULL;

    if (o->op_type == 0)
        return (o->op_flags & OPf_KIDS) ? OPc_UNOP : OPc_BASEOP;

    if (o->op_type == OP_SASSIGN)
        return ((o->op_private & OPpASSIGN_BACKWARDS) ? OPc_UNOP : OPc_BINOP);

    if (o->op_type == OP_AELEMFAST) {
#if PERL_VERSION <= 14
        if (o->op_flags & OPf_SPECIAL)
            return OPc_BASEOP;
        else
#endif
#ifdef USE_ITHREADS
            return OPc_PADOP;
#else
            return OPc_SVOP;
#endif
    }
    
#ifdef USE_ITHREADS
    if (o->op_type == OP_GV || o->op_type == OP_GVSV ||
        o->op_type == OP_RCATLINE)
        return OPc_PADOP;
#endif

    if (o->op_type == OP_CUSTOM)
        custom = 1;

    switch (OP_CLASS(o)) {
    case OA_BASEOP:
        return OPc_BASEOP;

    case OA_UNOP:
        return OPc_UNOP;

    case OA_BINOP:
        return OPc_BINOP;

    case OA_LOGOP:
        return OPc_LOGOP;

    case OA_LISTOP:
        return OPc_LISTOP;

    case OA_PMOP:
        return OPc_PMOP;

    case OA_SVOP:
        return OPc_SVOP;

    case OA_PADOP:
        return OPc_PADOP;

    case OA_PVOP_OR_SVOP:
        /*
         * Character translations (tr///) are usually a PVOP, keeping a 
         * pointer to a table of shorts used to look up translations.
         * Under utf8, however, a simple table isn't practical; instead,
         * the OP is an SVOP (or, under threads, a PADOP),
         * and the SV is a reference to a swash
         * (i.e., an RV pointing to an HV).
         */
        return (!custom &&
                   (o->op_private & (OPpTRANS_TO_UTF|OPpTRANS_FROM_UTF))
               )
#if  defined(USE_ITHREADS)
                ? OPc_PADOP : OPc_PVOP;
#else
                ? OPc_SVOP : OPc_PVOP;
#endif

    case OA_LOOP:
        return OPc_LOOP;

    case OA_COP:
        return OPc_COP;

    case OA_BASEOP_OR_UNOP:
        /*
         * UNI(OP_foo) in toke.c returns token UNI or FUNC1 depending on
         * whether parens were seen. perly.y uses OPf_SPECIAL to
         * signal whether a BASEOP had empty parens or none.
         * Some other UNOPs are created later, though, so the best
         * test is OPf_KIDS, which is set in newUNOP.
         */
        return (o->op_flags & OPf_KIDS) ? OPc_UNOP : OPc_BASEOP;

    case OA_FILESTATOP:
        /*
         * The file stat OPs are created via UNI(OP_foo) in toke.c but use
         * the OPf_REF flag to distinguish between OP types instead of the
         * usual OPf_SPECIAL flag. As usual, if OPf_KIDS is set, then we
         * return OPc_UNOP so that walkoptree can find our children. If
         * OPf_KIDS is not set then we check OPf_REF. Without OPf_REF set
         * (no argument to the operator) it's an OP; with OPf_REF set it's
         * an SVOP (and op_sv is the GV for the filehandle argument).
         */
        return ((o->op_flags & OPf_KIDS) ? OPc_UNOP :
#ifdef USE_ITHREADS
                (o->op_flags & OPf_REF) ? OPc_PADOP : OPc_BASEOP);
#else
                (o->op_flags & OPf_REF) ? OPc_SVOP : OPc_BASEOP);
#endif
    case OA_LOOPEXOP:
        /*
         * next, last, redo, dump and goto use OPf_SPECIAL to indicate that a
         * label was omitted (in which case it's a BASEOP) or else a term was
         * seen. In this last case, all except goto are definitely PVOP but
         * goto is either a PVOP (with an ordinary constant label), an UNOP
         * with OPf_STACKED (with a non-constant non-sub) or an UNOP for
         * OP_REFGEN (with goto &sub) in which case OPf_STACKED also seems to
         * get set.
         */
        if (o->op_flags & OPf_STACKED)
            return OPc_UNOP;
        else if (o->op_flags & OPf_SPECIAL)
            return OPc_BASEOP;
        else
            return OPc_PVOP;
    }
    warn("can't determine class of operator %s, assuming BASEOP\n",
         OP_NAME(o));
    return OPc_BASEOP;
}

const char *cc_opclassname(pTHX_ const OP *o)
{
    return opclassnames[cc_opclass(aTHX_ o)];
}
