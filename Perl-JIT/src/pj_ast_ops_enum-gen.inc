#ifndef PJ_AST_OPS_ENUM_GEN_INC_
#define PJ_AST_OPS_ENUM_GEN_INC_
// WARNING: Do not modify this file, it is generated!
// Modify the generating script author_tools/regen_opcodes.pl and its data file
// "author_tools/opcodes" instead!

typedef enum {

  // Op class: pj_opc_unop
  pj_unop_empty,
  pj_unop_time,
  pj_unop_times,
  pj_unop_localtime,
  pj_unop_gmtime,
  pj_unop_alarm,
  pj_unop_sleep,
  pj_unop_negate,
  pj_unop_sin,
  pj_unop_cos,
  pj_unop_abs,
  pj_unop_sqrt,
  pj_unop_log,
  pj_unop_exp,
  pj_unop_perl_int,
  pj_unop_bitwise_not,
  pj_unop_bool_not,
  pj_unop_defined,
  pj_unop_rand,
  pj_unop_srand,
  pj_unop_hex,
  pj_unop_oct,
  pj_unop_length,
  pj_unop_ord,
  pj_unop_chr,
  pj_unop_lc,
  pj_unop_uc,
  pj_unop_lcfirst,
  pj_unop_ucfirst,
  pj_unop_quotemeta,
  pj_unop_preinc,
  pj_unop_postinc,
  pj_unop_predec,
  pj_unop_postdec,
  pj_unop_undef,
  pj_unop_sv_deref,
  pj_unop_sv_ref,
  pj_unop_getc,
  pj_unop_array_len,

  // Op class: pj_opc_binop
  pj_binop_add,
  pj_binop_subtract,
  pj_binop_multiply,
  pj_binop_divide,
  pj_binop_modulo,
  pj_binop_atan2,
  pj_binop_pow,
  pj_binop_left_shift,
  pj_binop_right_shift,
  pj_binop_bitwise_and,
  pj_binop_bitwise_or,
  pj_binop_bitwise_xor,
  pj_binop_num_eq,
  pj_binop_num_ne,
  pj_binop_num_lt,
  pj_binop_num_le,
  pj_binop_num_gt,
  pj_binop_num_ge,
  pj_binop_num_cmp,
  pj_binop_str_eq,
  pj_binop_str_ne,
  pj_binop_str_lt,
  pj_binop_str_le,
  pj_binop_str_gt,
  pj_binop_str_ge,
  pj_binop_str_cmp,
  pj_binop_bool_and,
  pj_binop_bool_or,
  pj_binop_definedor,
  pj_binop_concat,
  pj_binop_sassign,

  // Op class: pj_opc_listop
  pj_listop_ternary,
  pj_listop_substr,
  pj_listop_chop,
  pj_listop_chomp,
  pj_listop_vec,
  pj_listop_sprintf,
  pj_listop_printf,
  pj_listop_print,
  pj_listop_say,
  pj_listop_join,
  pj_listop_read,
  pj_listop_list2scalar,

  // Op class ranges

  pj_unop_FIRST = pj_unop_empty,
  pj_unop_LAST  = pj_unop_array_len,

  pj_binop_FIRST = pj_binop_add,
  pj_binop_LAST  = pj_binop_sassign,

  pj_listop_FIRST = pj_listop_ternary,
  pj_listop_LAST  = pj_listop_list2scalar,

  // Global op range:
  pj_op_FIRST = pj_unop_empty,
  pj_op_LAST = pj_listop_list2scalar
} pj_op_type;

#endif
