#include "pj_types.h"

#include <stdio.h>
#include <cstdlib>
#include <string.h>
#include <tr1/unordered_map>
#include <typeinfo>

using namespace std;
using namespace std::tr1;
using namespace PerlJIT;
using namespace PerlJIT::AST;

const Scalar PerlJIT::AST::OPAQUE_T(pj_opaque_type);
const Scalar PerlJIT::AST::DOUBLE_T(pj_double_type);
const Scalar PerlJIT::AST::INT_T(pj_int_type);
const Scalar PerlJIT::AST::UNSIGNED_INT_T(pj_uint_type);
const Scalar PerlJIT::AST::UNSPECIFIED_T(pj_unspecified_type);
const Scalar PerlJIT::AST::ANY_T(pj_any_type);
const Scalar PerlJIT::AST::SCALAR_T(pj_scalar_type);

#define ANY         "Any"
#define OPAQUE      "Opaque"
#define STRING      "String"
#define DOUBLE      "Double"
#define INT         "Int"
#define UINT        "UnsignedInt"

Type::~Type()
{
}


static Type *
minimal_covering_type_internal(const Type &left, const Type &right)
{
  pj_type_id left_tag = left.tag();
  pj_type_id right_tag = right.tag();

  // pj_unspecified_type and pj_any_type are the same for this logic
  if (left_tag == pj_any_type)
    left_tag = pj_unspecified_type;
  if (right_tag == pj_any_type)
    right_tag = pj_unspecified_type;

  // short-circuit
  if (left_tag == right_tag) {
    if (!left.is_composite())
      return left.clone();

    // Array or Hash
    Type *left_elem;
    Type *right_elem;

    if (left_tag == pj_array_type) {
      const Array &left_comp = dynamic_cast<const Array &>(left);
      const Array &right_comp = dynamic_cast<const Array &>(right);
      left_elem = left_comp.element();
      right_elem = right_comp.element();
    }
    else {
      const Hash &left_comp = dynamic_cast<const Hash &>(left);
      const Hash &right_comp = dynamic_cast<const Hash &>(right);
      left_elem = left_comp.element();
      right_elem = right_comp.element();
    }
    //printf("# Recursing for %s and %s\n", left_elem->to_string().c_str(), right_elem->to_string().c_str());
    Type *res = minimal_covering_type_internal(*left_elem, *right_elem);
    if (res == NULL)
      return res;
    if (res->equals(left_elem)) {
      delete res;
      return left.clone();
    }
    else {
      delete res;
      return right.clone();
    }
  }

  // incompatible combinations with unique types
  else if (left.is_array() || right.is_array())
    return NULL;
  else if (left.is_hash() || right.is_hash())
    return NULL;
  else if (left_tag == pj_gv_type || right_tag == pj_gv_type)
    return NULL; // FIXME GV relations a bit wobbly.

  // short-circuit unspecified type with other scalar type
  else if (left.is_unspecified())
    return right.clone();
  else if (right.is_unspecified())
    return left.clone();

  // Only descendants of opaque (== opaque scalar) left
  else if (left_tag == pj_opaque_type)
    return left.clone();
  else if (right_tag == pj_opaque_type)
    return right.clone();

  // Only descendants of scalar left
  else if (left_tag == pj_scalar_type)
    return left.clone();
  else if (right_tag == pj_scalar_type)
    return right.clone();

  // If *one* of them is a string, then upgrade to full scalar type
  else if (left_tag == pj_string_type || right_tag == pj_string_type)
    return new Scalar(pj_scalar_type);

  // FIXME this is debatable. Is double really >> Int/UInt?
  // Now, all that should be left is double, int, uint. And we already know
  // that the two types are not the same. Since neither int >> uint or vice
  // versa, we'll just upgrade to DOUBLE here.
  // We'll enumerate all those cases just so that we can catch any bad
  // assumptions in the else case instead of falling through to this case
  // for things that weren't appropriately covered above.
  else if (    (   left_tag == pj_double_type
                || left_tag == pj_int_type
                || left_tag == pj_uint_type)
            && (   right_tag == pj_double_type
                || right_tag == pj_int_type
                || right_tag == pj_uint_type) )
    return new Scalar(pj_double_type);

  else {
    printf("There's types in minimal_covering_type that aren't "
           "handled correctly at all: left=%s right=%s",
           left.to_string().c_str(), right.to_string().c_str());
    abort();
    return NULL;
  }
  abort(); // never reached
  return NULL;
}


static unordered_map<string, Type *>
unique_types(const vector<Type *> &types)
{
  unordered_map<string, Type *> uniq;

  const unsigned int n = types.size();
  for (unsigned int i = 0; i < n; ++i)
    uniq[types[i]->to_string()] = types[i];

  return uniq;
}


namespace PerlJIT {
  namespace AST {
    Type *
    minimal_covering_type(const vector<Type *> &types)
    {
      unordered_map<string, Type *> uniq_types = unique_types(types);

      Type *minimal_type = NULL;
      unordered_map<string, Type *>::iterator it = uniq_types.begin();
      unordered_map<string, Type *>::iterator end = uniq_types.end();
      for (; it != end; ++it) {
        Type *t = it->second;

        if (minimal_type == NULL) {
          minimal_type = t->clone();
        }
        else if (!minimal_type->equals(t)) {
          //printf("Cmp %s and %s\n", minimal_type->to_string().c_str(), t->to_string().c_str());
          Type *new_type = minimal_covering_type_internal(*minimal_type, *t);
          //printf("Result: %s\n", new_type == NULL ? "NULL" : new_type->to_string().c_str());
          if (new_type != minimal_type)
            delete minimal_type;
          if (new_type == NULL)
            return NULL;
          minimal_type = new_type;
        }
      }

      return minimal_type;
    }
  } // end namespace AST
} // end namespace PerlJIT


Scalar::Scalar(pj_type_id tag) :
  _tag(tag)
{
}

Type *
Scalar::clone() const
{
  return new Scalar(_tag);
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
  return (_tag == pj_scalar_type || _tag == pj_gv_type || _tag == pj_opaque_type);
}

bool Scalar::is_integer() const
{
  return (_tag == pj_int_type || _tag == pj_uint_type);
}

bool Scalar::is_unsigned() const
{
  return _tag == pj_uint_type;
}

bool Scalar::is_numeric() const
{
  return (   _tag == pj_int_type
          || _tag == pj_uint_type
          || _tag == pj_double_type);
}

bool Scalar::equals(const Type *other) const
{
  const Scalar *o = dynamic_cast<const Scalar *>(other);

  return o && o->_tag == _tag;
}

#define PRINT_TYPE(value, string) \
  case value: \
    return string;

string Scalar::to_string() const
{
  switch (_tag) {
    PRINT_TYPE(pj_unspecified_type, "Any"); // FIXME shady
    PRINT_TYPE(pj_any_type, "Any");
    PRINT_TYPE(pj_scalar_type, "Scalar");
    PRINT_TYPE(pj_gv_type, "GV");
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

Type *
Array::clone() const
{
  return new Array(_element->clone());
}

pj_type_id Array::tag() const
{
  return pj_array_type;
}

Type *Array::element() const
{
  return _element;
}

bool Array::equals(const Type *other) const
{
  const Array *o = dynamic_cast<const Array *>(other);

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

Type *
Hash::clone() const
{
  return new Hash(_element->clone());
}

pj_type_id Hash::tag() const
{
  return pj_hash_type;
}

Type *Hash::element() const
{
  return _element;
}

bool Hash::equals(const Type *other) const
{
  const Hash *o = dynamic_cast<const Hash *>(other);

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
