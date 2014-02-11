#ifndef PJ_XSP_TYPEDEFS_H_
#define PJ_XSP_TYPEDEFS_H_

namespace PerlJIT {
  namespace AST { }
}

// make the names available in a separate namespace, to avoid
// a lot of %name directives in XS++
namespace Perl {
  namespace JIT {
    namespace AST {
      using namespace PerlJIT::AST;
    }
  }
}

namespace Perl {
  namespace JIT {
    using namespace PerlJIT;
  }
}

#endif // PJ_XSP_TYPEDEFS_H_
