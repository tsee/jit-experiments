#include "types.h"

#include <string.h>

using namespace PerlJIT;
using namespace PerlJIT::AST;

Type::~Type()
{
}

Scalar::Scalar(pj_type_id tag) :
  _tag(tag)
{
}

pj_type_id Scalar::tag() const
{
  return _tag;
}

#define SCALAR      "Scalar"
#define STRING      "String"
#define DOUBLE      "Double"
#define INT         "Int"
#define UINT        "UnsignedInt"

#define PARSE_SCALAR(name, type) \
  if (strcmp(string, name) == 0) \
    return new Scalar(type)

#define CHECK_SCALAR(name) \
  if (strcmp(string, name) == 0) \
    return true

namespace PerlJIT {
  namespace AST {
    Type *parse_type(const char *string)
    {
      PARSE_SCALAR(SCALAR, pj_scalar_type);
      PARSE_SCALAR(STRING, pj_string_type);
      PARSE_SCALAR(DOUBLE, pj_double_type);
      PARSE_SCALAR(INT, pj_int_type);
      PARSE_SCALAR(UINT, pj_uint_type);

      return 0;
    }

    bool is_type(const char *string)
    {
      CHECK_SCALAR(SCALAR);
      CHECK_SCALAR(STRING);
      CHECK_SCALAR(DOUBLE);
      CHECK_SCALAR(INT);
      CHECK_SCALAR(UINT);

      return false;
    }
  }
}
