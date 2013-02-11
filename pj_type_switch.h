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
    default: \
      abort(); \
    } \
  }
