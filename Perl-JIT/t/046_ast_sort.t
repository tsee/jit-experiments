#!/usr/bin/env perl

use t::lib::Perl::JIT::ASTTest;
use t::lib::Perl::JIT::Test;

my (@a, @b);
my ($x, $y);
# The first and second test are identical,
# except that the first (failing) test als tests
# some additional structure around the sort.
# In a nutshell, I don't think there should be a List()
# node in the AST.

ast_contains(
  sub { @a = sort @b },
  ast_sort(
    reverse => 0,
    numeric => 0,
    args => [ast_lexical('@b')],
  ),
  "Simple bare sort"
);

TODO: {
  local $TODO = "AST surrounding Sort is still messy";
  ast_contains(
    sub { @a = sort @b },
    ast_binop(
      pj_binop_aassign,
      ast_lexical('@a'),
      ast_sort(
        reverse => 0,
        numeric => 0,
        args => [ast_lexical('@b')],
      )
    ),
    "Simple bare sort with surrounding aassign"
  );
}

TODO: {
  local $TODO = "reverse sort is not correctly folded in yet !?";
  ast_contains(
    sub { @a = reverse sort @b },
    ast_sort(
      reverse => 1,
      numeric => 0,
      args => [ast_lexical('@b')],
    ),
    "'reverse sort' without block"
  );
}

ast_contains(
  sub { @a = sort {$a cmp $b} @b },
  ast_sort(
    reverse => 0,
    numeric => 0,
    args => [ast_lexical('@b')],
  ),
  "Sort with optimized string comparison block"
);

ast_contains(
  sub { @a = sort {$b cmp $a} @b },
  ast_sort(
    reverse => 1,
    numeric => 0,
    args => [ast_lexical('@b')],
  ),
  "Sort with reversed, optimized string comparison block"
);

TODO: {
  local $TODO = "reverse sort is not correctly folded in yet !?";
  ast_contains(
    sub { @a = reverse sort {$b cmp $a} @b },
    ast_sort(
      reverse => 0,
      numeric => 0,
      args => [ast_lexical('@b')],
    ),
    "'reverse sort' with reversed, optimized string comparsion block"
  );
}

ast_contains(
  sub { @a = sort {$b <=> $a} @b },
  ast_sort(
    reverse => 1,
    numeric => 1,
    args => [ast_lexical('@b')],
  ),
  "Sort with reversed, optimized numeric comparsion block"
);

ast_contains(
  sub { @a = sort {$x} @b },
  ast_sort(
    reverse => 0,
    numeric => 0,
    args => [ast_lexical('@b')],
    cmp => ast_block( ast_lexical('$x') ),
  ),
  "Sort with non-optimized cmp block"
);

ast_contains(
  sub { @a = sort {$x} (1, 2) },
  ast_sort(
    reverse => 0,
    numeric => 0,
    args => [ast_constant(1), ast_constant(2)],
    cmp => ast_block( ast_lexical('$x') ),
  ),
  "Sort with input list"
);

sub foo {$x}
ast_contains(
  sub { @a = sort foo @b },
  ast_sort(
    reverse => 0,
    numeric => 0,
    args => [ast_lexical('@b')],
    cmp => ast_constant("foo", pj_string_type),
  ),
  "Sort with named cmp block"
);


done_testing();
