/* WARNING: Do not modify this file, it is generated!
 * Modify the generating script make_function_invoker.pl instead! */

#define PJ_TYPE_SWITCH(type, args, nargs, retval) \
  { \
  type *a = (type *)args; \
  switch (nargs) { \
    case 0: { \
      type (*f)(void) = (type (*)(void))fptr; \
      *((type *)retval) = f(); \
      break; } \
    case 1: { \
      type (*f)(type) = (type (*)(type))fptr; \
      *((type *)retval) = f(a[0]); \
      break; } \
    case 2: { \
      type (*f)(type, type) = (type (*)(type, type))fptr; \
      *((type *)retval) = f(a[0], a[1]); \
      break; } \
    case 3: { \
      type (*f)(type, type, type) = (type (*)(type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2]); \
      break; } \
    case 4: { \
      type (*f)(type, type, type, type) = (type (*)(type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3]); \
      break; } \
    case 5: { \
      type (*f)(type, type, type, type, type) = (type (*)(type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4]); \
      break; } \
    case 6: { \
      type (*f)(type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5]); \
      break; } \
    case 7: { \
      type (*f)(type, type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6]); \
      break; } \
    case 8: { \
      type (*f)(type, type, type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]); \
      break; } \
    case 9: { \
      type (*f)(type, type, type, type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8]); \
      break; } \
    case 10: { \
      type (*f)(type, type, type, type, type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9]); \
      break; } \
    case 11: { \
      type (*f)(type, type, type, type, type, type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10]); \
      break; } \
    case 12: { \
      type (*f)(type, type, type, type, type, type, type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type, type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11]); \
      break; } \
    case 13: { \
      type (*f)(type, type, type, type, type, type, type, type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type, type, type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12]); \
      break; } \
    case 14: { \
      type (*f)(type, type, type, type, type, type, type, type, type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type, type, type, type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13]); \
      break; } \
    case 15: { \
      type (*f)(type, type, type, type, type, type, type, type, type, type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type, type, type, type, type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14]); \
      break; } \
    case 16: { \
      type (*f)(type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15]); \
      break; } \
    case 17: { \
      type (*f)(type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15], a[16]); \
      break; } \
    case 18: { \
      type (*f)(type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15], a[16], a[17]); \
      break; } \
    case 19: { \
      type (*f)(type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15], a[16], a[17], a[18]); \
      break; } \
    case 20: { \
      type (*f)(type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type) = (type (*)(type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type, type))fptr; \
      *((type *)retval) = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15], a[16], a[17], a[18], a[19]); \
      break; } \
    default: \
      abort(); \
    } \
  }
