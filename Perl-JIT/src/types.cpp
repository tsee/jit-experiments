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

namespace PerlJIT {
  namespace AST {
    Type *parse_type(const char *string)
    {
      if (strcmp(string, "Scalar") == 0)
        return new Scalar(pj_scalar_type);
      if (strcmp(string, "String") == 0)
        return new Scalar(pj_string_type);
      if (strcmp(string, "Double") == 0)
        return new Scalar(pj_double_type);
      if (strcmp(string, "Int") == 0)
        return new Scalar(pj_int_type);
      if (strcmp(string, "UnsignedInt") == 0)
        return new Scalar(pj_uint_type);

      return 0;
    }
  }
}
