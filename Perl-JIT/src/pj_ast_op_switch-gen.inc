#ifndef PJ_AST_OP_SWITCH_GEN_INC_
#define PJ_AST_OP_SWITCH_GEN_INC_
// WARNING: Do not modify this file, it is generated!
// Modify the generating script author_tools/regen_opcodes.pl and its data file
// "author_tools/opcodes" instead!

// Macros to determine which Perl OPs to ASTify

#define IS_AST_COMPATIBLE_ROOT_OP_TYPE(otype) ( \
     otype == OP_NEGATE \
  || otype == OP_SIN \
  || otype == OP_COS \
  || otype == OP_ABS \
  || otype == OP_SQRT \
  || otype == OP_LOG \
  || otype == OP_EXP \
  || otype == OP_INT \
  || otype == OP_COMPLEMENT \
  || otype == OP_NOT \
  || otype == OP_DEFINED \
  || otype == OP_RAND \
  || otype == OP_SRAND \
  || otype == OP_HEX \
  || otype == OP_OCT \
  || otype == OP_LENGTH \
  || otype == OP_ORD \
  || otype == OP_CHR \
  || otype == OP_LC \
  || otype == OP_UC \
  || otype == OP_LCFIRST \
  || otype == OP_UCFIRST \
  || otype == OP_QUOTEMETA \
  || otype == OP_PREINC \
  || otype == OP_POSTINC \
  || otype == OP_PREDEC \
  || otype == OP_POSTDEC \
  || otype == OP_UNDEF \
  || otype == OP_RV2SV \
  || otype == OP_SREFGEN \
  || otype == OP_ADD \
  || otype == OP_SUBTRACT \
  || otype == OP_MULTIPLY \
  || otype == OP_DIVIDE \
  || otype == OP_MODULO \
  || otype == OP_ATAN2 \
  || otype == OP_POW \
  || otype == OP_LEFT_SHIFT \
  || otype == OP_RIGHT_SHIFT \
  || otype == OP_BIT_AND \
  || otype == OP_BIT_OR \
  || otype == OP_BIT_XOR \
  || otype == OP_EQ \
  || otype == OP_NE \
  || otype == OP_LT \
  || otype == OP_LE \
  || otype == OP_GT \
  || otype == OP_GE \
  || otype == OP_NCMP \
  || otype == OP_SEQ \
  || otype == OP_SNE \
  || otype == OP_SLT \
  || otype == OP_SLE \
  || otype == OP_SGT \
  || otype == OP_SGE \
  || otype == OP_SCMP \
  || otype == OP_COND_EXPR \
  || otype == OP_SUBSTR)


#define IS_AST_COMPATIBLE_OP_TYPE(otype) ( \
     otype == OP_CONST \
  || otype == OP_PADSV \
  || otype == OP_NULL \
  || otype == OP_GVSV \
  || otype == OP_NEGATE \
  || otype == OP_SIN \
  || otype == OP_COS \
  || otype == OP_ABS \
  || otype == OP_SQRT \
  || otype == OP_LOG \
  || otype == OP_EXP \
  || otype == OP_INT \
  || otype == OP_COMPLEMENT \
  || otype == OP_NOT \
  || otype == OP_DEFINED \
  || otype == OP_RAND \
  || otype == OP_SRAND \
  || otype == OP_HEX \
  || otype == OP_OCT \
  || otype == OP_LENGTH \
  || otype == OP_ORD \
  || otype == OP_CHR \
  || otype == OP_LC \
  || otype == OP_UC \
  || otype == OP_LCFIRST \
  || otype == OP_UCFIRST \
  || otype == OP_QUOTEMETA \
  || otype == OP_PREINC \
  || otype == OP_POSTINC \
  || otype == OP_PREDEC \
  || otype == OP_POSTDEC \
  || otype == OP_UNDEF \
  || otype == OP_RV2SV \
  || otype == OP_SREFGEN \
  || otype == OP_ADD \
  || otype == OP_SUBTRACT \
  || otype == OP_MULTIPLY \
  || otype == OP_DIVIDE \
  || otype == OP_MODULO \
  || otype == OP_ATAN2 \
  || otype == OP_POW \
  || otype == OP_LEFT_SHIFT \
  || otype == OP_RIGHT_SHIFT \
  || otype == OP_BIT_AND \
  || otype == OP_BIT_OR \
  || otype == OP_BIT_XOR \
  || otype == OP_EQ \
  || otype == OP_NE \
  || otype == OP_LT \
  || otype == OP_LE \
  || otype == OP_GT \
  || otype == OP_GE \
  || otype == OP_NCMP \
  || otype == OP_SEQ \
  || otype == OP_SNE \
  || otype == OP_SLT \
  || otype == OP_SLE \
  || otype == OP_SGT \
  || otype == OP_SGE \
  || otype == OP_SCMP \
  || otype == OP_AND \
  || otype == OP_OR \
  || otype == OP_DOR \
  || otype == OP_COND_EXPR \
  || otype == OP_SUBSTR)



#endif
