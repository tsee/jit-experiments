#include "types.h"

#include <stdio.h>
#include <cstdlib>
#include <string.h>

using namespace std;
using namespace PerlJIT;
using namespace PerlJIT::AST;

#define ANY         "Any"
#define OPAQUE      "Opaque"
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

bool Scalar::is_unspecified() const
{
  return _tag == pj_unspecified_type;
}

bool Scalar::is_opaque() const
{
  return _tag == pj_opaque_type;
}

bool Scalar::is_xv() const
{
  return (_tag == pj_sv_type || _tag == pj_gv_type);
}

bool Scalar::is_integer() const
{
  return (_tag == pj_int_type || _tag == pj_uint_type);
}

bool Scalar::is_numeric() const
{
  return (   _tag == pj_int_type
          || _tag == pj_uint_type
          || _tag == pj_double_type);
}

bool Scalar::equals(Type *other) const
{
  Scalar *o = dynamic_cast<Scalar *>(other);

  return o && o->_tag == _tag;
}

#define PRINT_TYPE(value, string) \
  case value: \
    return string;

string Scalar::to_string() const
{
  switch (_tag) {
    PRINT_TYPE(pj_unspecified_type, "Any");
    PRINT_TYPE(pj_opaque_type, OPAQUE);
    PRINT_TYPE(pj_string_type, STRING);
    PRINT_TYPE(pj_double_type, DOUBLE);
    PRINT_TYPE(pj_int_type, INT);
    PRINT_TYPE(pj_uint_type, UINT);
  default:
    abort(); /* shut up compiler warnings */
  }
  return "InvalidScalarType";
}

Array::Array(Type *element) :
  _element(element)
{
}

pj_type_id Array::tag() const
{
  return pj_array_type;
}

Type *Array::element() const
{
  return _element;
}

bool Array::equals(Type *other) const
{
  Array *o = dynamic_cast<Array *>(other);

  return o && o->_element->equals(_element);
}

string Array::to_string() const
{
  return "Array[" + _element->to_string() + "]";
}

Hash::Hash(Type *element) :
  _element(element)
{
}

pj_type_id Hash::tag() const
{
  return pj_hash_type;
}

Type *Hash::element() const
{
  return _element;
}

bool Hash::equals(Type *other) const
{
  Hash *o = dynamic_cast<Hash *>(other);

  return o && o->_element->equals(_element);
}

string Hash::to_string() const
{
  return "Hash[" + _element->to_string() + "]";
}

#define PARSE_SCALAR(name, type) \
  if (starts_with(str, name)) { \
    rest = str.substr(strlen(name)); \
    return new Scalar(type); \
  }

#define PARSE_NESTED(name, type) \
  if (Type *t = parse_nested<type>(name "[", str, rest)) \
    return t

static inline bool
starts_with(const string &str, const string &substr)
{
  return str.rfind(substr, 0) == 0;
}

namespace PerlJIT {
  namespace AST {
    Type *parse_type_part(const string &str, string &rest);

    template<class T>
    Type *parse_nested(const string &start, const string &str, string &rest)
    {
      if (starts_with(str, start)) {
        string tail;
        Type *element = parse_type_part(str.substr(start.size()), tail);

        if (element && tail.length() && tail[0] == ']') {
          rest = tail.substr(1);
          return new T(element);
        }

        delete element;
        return 0;
      }
      return 0;
    }

    Type *parse_type_part(const string &str, string &rest)
    {
      PARSE_SCALAR(ANY, pj_unspecified_type);
      PARSE_SCALAR(OPAQUE, pj_opaque_type);
      PARSE_SCALAR(STRING, pj_string_type);
      PARSE_SCALAR(DOUBLE, pj_double_type);
      PARSE_SCALAR(INT, pj_int_type);
      PARSE_SCALAR(UINT, pj_uint_type);
      PARSE_NESTED("Array", Array);
      PARSE_NESTED("Hash", Hash);

      return 0;
    }

    Type *parse_type(const string &str)
    {
      string rest;
      Type *type = parse_type_part(str, rest);

      if (rest.size()) {
        delete type;
        return 0;
      }
      else
        return type;
    }
  }
}
