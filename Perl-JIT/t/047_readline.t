#!/usr/bin/env perl

use t::lib::Perl::JIT::ASTTest;
use t::lib::Perl::JIT::Test;

my ($x, $y);
ast_contains(
  sub { $x .= <F> },
      ast_unop(
        pj_unop_rcatline,
        ast_global('$x')
      ),
  "rcatline optimization"
);

TODO: {
  local $TODO = "Steffen was too lazy to write the rest of the tests";
  fail("Need basic readline tests");
  fail('Need readline test for "special" flag ($x = <F>?)');
}

done_testing();
