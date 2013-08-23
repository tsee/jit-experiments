#ifndef PJ_AST_OPS_ENUM_GEN_INC_
#define PJ_AST_OPS_ENUM_GEN_INC_
// WARNING: Do not modify this file, it is generated!
// Modify the generating script author_tools/regen_opcodes.pl and its data file
// "author_tools/opcodes" instead!

typedef enum {

  // Op class: pj_opc_unop
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
  pj_binop_bool_and,
  pj_binop_bool_or,

  // Op class: pj_opc_listop
  pj_listop_ternary,

  // Op class ranges

  pj_unop_FIRST = pj_unop_negate,
  pj_unop_LAST  = pj_unop_defined,

  pj_binop_FIRST = pj_binop_add,
  pj_binop_LAST  = pj_binop_bool_or,

  pj_listop_FIRST = pj_listop_ternary,
  pj_listop_LAST  = pj_listop_ternary,

  // Global op range:
  pj_op_FIRST = pj_unop_negate,
  pj_op_LAST = pj_listop_ternary
} pj_op_type;

#endif