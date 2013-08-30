#include "types.h"

using namespace std;
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

#define ANY         "Any"
#define SCALAR      "Scalar"
#define STRING      "String"
#define DOUBLE      "Double"
#define INT         "Int"
#define UINT        "UnsignedInt"

bool Scalar::equals(Type *other) const
{
  Scalar *o = dynamic_cast<Scalar *>(other);

  return o && o->_tag == _tag;
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
