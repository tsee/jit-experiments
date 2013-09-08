#ifndef PJ_TYPES_H_
#define PJ_TYPES_H_

#include <string>

// TODO should this have a pj_undef_type? If so, change the AST::UndefConstant class.
enum pj_type_id {
  pj_unspecified_type, // Unknown type that is some sort of Perl SV* (eligible for optimization)
  pj_any_type,         // Used internally for passing in desired return types during code generation.
                       // "Any" means any return type is equally valid/desired.
  pj_scalar_type,      // Explicit Perl Scalar (not any SV, but any scalar SV)
  pj_gv_type,          // Actual GV type.
  pj_opaque_type,      // Explicitly opaque Perl SV "must not treat as anything but an unknown Perl thing"
                       // Interaction must support ties/overloading.
  pj_array_type,       // Explicit array type (commonly a Perl AV)
  pj_hash_type,        // Explicit hash type (commonly a Perl HV)
  pj_string_type,      // Explicit JIT string type (and we haven't defined what that means)
  pj_double_type,      // Explicit JIT floating point type
  pj_int_type,         // Explicit JIT int type
  pj_uint_type         // Explicit JIT uint type
};

namespace PerlJIT {
  namespace AST {
    class Type {
    public:
      virtual ~Type();
      virtual Type *clone() const = 0;

      virtual pj_type_id tag() const = 0;

      virtual bool equals(Type *other) const = 0;

      virtual bool is_scalar() const { return false; }
      virtual bool is_array() const { return false; }
      virtual bool is_hash() const { return false; }
      virtual bool is_opaque() const { return false; }
      virtual bool is_unspecified() const { return false; }

      virtual bool is_xv() const { return false; }
      virtual bool is_integer() const { return false; }
      virtual bool is_numeric() const { return false; }

      virtual std::string to_string() const = 0;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Type"; }
    };

    class Scalar : public Type {
    public:
      Scalar(pj_type_id tag);
      virtual Type *clone() const;

      virtual pj_type_id tag() const;

      virtual bool equals(Type *other) const;

      virtual bool is_scalar() const { return true; }
      virtual bool is_unspecified() const;
      virtual bool is_opaque() const;

      virtual bool is_xv() const;

      virtual bool is_integer() const;
      virtual bool is_numeric() const;

      virtual std::string to_string() const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Scalar"; }
    private:
      pj_type_id _tag;
    };

    class Array : public Type {
    public:
      Array(Type *element);
      virtual Type *clone() const;

      virtual pj_type_id tag() const;
      Type *element() const;

      virtual bool equals(Type *other) const;

      virtual bool is_array() const { return true; }

      virtual std::string to_string() const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Array"; }
    private:
      Type *_element;
    };

    class Hash : public Type {
    public:
      Hash(Type *element);
      virtual Type *clone() const;

      virtual pj_type_id tag() const;
      Type *element() const;

      virtual bool equals(Type *other) const;

      virtual bool is_hash() const { return true; }

      virtual std::string to_string() const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Hash"; }
    private:
      Type *_element;
    };

    Type *parse_type(const std::string &str);
  }
}

#endif // PJ_TYPES_H_
