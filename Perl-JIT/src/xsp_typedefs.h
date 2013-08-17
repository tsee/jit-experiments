#ifndef PJ_XSP_TYPEDEFS_H_
#define PJ_XSP_TYPEDEFS_H_

// make the names available in a separate namespace, to avoid
// a lot of %name directives in XS++
namespace Perl {
  namespace JIT {
    namespace AST {
      using namespace PerlJIT::AST;
    }
  }
}

#endif // PJ_XSP_TYPEDEFS_H_
