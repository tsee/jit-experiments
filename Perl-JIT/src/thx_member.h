#ifndef _DEVEL_STATPROFILER_THX_MEMBER
#define _DEVEL_STATPROFILER_THX_MEMBER

// Defines macros for use in classes that keep a Perl threading
// context around.
//
// Example:
// class Foo {
// public:
//   Foo(pTHX) {
//    SET_THX_MEMBER
//   }
//   void bar() {
//     // use aTHX here implicitly or explicitly
//   }
// private:
//   DECL_THX_MEMBER
// }

#include <EXTERN.h>
#include <perl.h>

#ifdef MULTIPLICITY
#   define DECL_THX_MEMBER tTHX my_perl;
#   define SET_THX_MEMBER this->my_perl = aTHX;
#else
#   define DECL_THX_MEMBER
#   define SET_THX_MEMBER
#endif

#ifdef PERL_IMPLICIT_CONTEXT
#   define DECL_CXT_MEMBER(type) type *my_cxtp;
#   define CXT_ARG(type) type *my_cxtp
#   define CXT_ARG_(type) type *my_cxtp,
#   define SET_CXT_MEMBER this->my_cxtp = aMY_CXT;
#else
#   define DECL_CXT_MEMBER(type)
#   define CXT_ARG(type)
#   define CXT_ARG_(type)
#   define SET_CXT_MEMBER
#endif

#endif
