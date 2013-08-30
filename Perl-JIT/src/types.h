#ifndef PJ_TYPES_H_
#define PJ_TYPES_H_

#include <string>

// TODO should this have a pj_undef_type? If so, change the AST::UndefConstant class.
enum pj_type_id {
  pj_unspecified_type,
  pj_opaque_type,
  pj_string_type,
  pj_double_type,
  pj_int_type,
  pj_uint_type
};

namespace PerlJIT {
  namespace AST {
    class Type {
    public:
      virtual ~Type();
      virtual pj_type_id tag() const = 0;

      virtual bool equals(Type *other) const = 0;

      virtual void dump() const = 0;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Type"; }
    };

    class Scalar : public Type {
    public:
      Scalar(pj_type_id tag);
      virtual pj_type_id tag() const;

      virtual bool equals(Type *other) const;

      virtual void dump() const;
      virtual const char *perl_class() const
        { return "Perl::JIT::AST::Scalar"; }
    private:
      pj_type_id _tag;
    };

    Type *parse_type(const std::string &str);
    bool is_type(const std::string &str);
  }
}

#endif // PJ_TYPES_H_
