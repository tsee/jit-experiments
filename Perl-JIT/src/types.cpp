#include "types.h"

#include <stdio.h>

using namespace std;
using namespace PerlJIT;
using namespace PerlJIT::AST;

#define ANY         "Any"
#define SCALAR      "Scalar"
#define STRING      "String"
#define DOUBLE      "Double"
#define INT         "Int"
#define UINT        "UnsignedInt"

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

bool Scalar::equals(Type *other) const
{
  Scalar *o = dynamic_cast<Scalar *>(other);

  return o && o->_tag == _tag;
}

#define PRINT_TYPE(value, string) \
  case value: \
    printf(string); \
    break;

void Scalar::dump() const
{
  switch (_tag) {
    PRINT_TYPE(pj_unspecified_type, "Any");
    PRINT_TYPE(pj_scalar_type, SCALAR);
    PRINT_TYPE(pj_string_type, STRING);
    PRINT_TYPE(pj_double_type, DOUBLE);
    PRINT_TYPE(pj_int_type, INT);
    PRINT_TYPE(pj_uint_type, UINT);
  }
}

#define PARSE_SCALAR(name, type) \
  if (str == string(name))       \
    return new Scalar(type)

#define CHECK_SCALAR(name) \
  if (str == string(name)) \
    return true

namespace PerlJIT {
  namespace AST {
    Type *parse_type(const string &str)
    {
      PARSE_SCALAR(ANY, pj_unspecified_type);
      PARSE_SCALAR(SCALAR, pj_scalar_type);
      PARSE_SCALAR(STRING, pj_string_type);
      PARSE_SCALAR(DOUBLE, pj_double_type);
      PARSE_SCALAR(INT, pj_int_type);
      PARSE_SCALAR(UINT, pj_uint_type);

      return 0;
    }

    bool is_type(const string &str)
    {
      CHECK_SCALAR(ANY);
      CHECK_SCALAR(SCALAR);
      CHECK_SCALAR(STRING);
      CHECK_SCALAR(DOUBLE);
      CHECK_SCALAR(INT);
      CHECK_SCALAR(UINT);

      return false;
    }
  }
}
