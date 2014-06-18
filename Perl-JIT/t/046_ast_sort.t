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
    algo => "Merge",
    args => [ast_lexical('@b')],
  ),
  "Simple bare sort"
);

ast_contains(
  sub { @a = sort @b },
  ast_binop(
    pj_binop_aassign,
    ast_list(ast_lexical('@a')),
    ast_list(ast_sort(
      reverse => 0,
      numeric => 0,
      algo => "Merge",
      args => [ast_lexical('@b')],
    ))
  ),
  "Simple bare sort with surrounding aassign"
);

ast_contains(
  sub { @a = reverse sort @b },
  ast_sort(
    reverse => 1,
    algo => "Merge",
    numeric => 0,
    args => [ast_lexical('@b')],
  ),
  "'reverse sort' without block"
);

ast_contains(
  sub { @a = sort {$a cmp $b} @b },
  ast_sort(
    reverse => 0,
    algo => "Merge",
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
    algo => "Merge",
    args => [ast_lexical('@b')],
  ),
  "Sort with reversed, optimized string comparison block"
);

ast_contains(
  sub { @a = reverse sort {$b cmp $a} @b },
  ast_sort(
    reverse => 0,
    numeric => 0,
    algo => "Merge",
    args => [ast_lexical('@b')],
  ),
  "'reverse sort' with reversed, optimized string comparsion block"
);

ast_contains(
  sub { @a = sort {$b <=> $a} @b },
  ast_sort(
    reverse => 1,
    numeric => 1,
    algo => "Merge",
    args => [ast_lexical('@b')],
  ),
  "Sort with reversed, optimized numeric comparsion block"
);

ast_contains(
  sub { @a = sort {$x} @b },
  ast_sort(
    reverse => 0,
    numeric => 0,
    algo => "Merge",
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
    algo => "Merge",
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
    algo => "Merge",
    args => [ast_lexical('@b')],
    cmp => ast_constant("foo", pj_string_type),
  ),
  "Sort with named cmp block"
);

ast_contains(
  sub { @a = reverse sort foo @b },
  ast_sort(
    reverse => 1,
    numeric => 0,
    algo => "Merge",
    args => [ast_lexical('@b')],
    cmp => ast_constant("foo", pj_string_type),
  ),
  "Sort with named cmp block"
);

ast_contains(
  sub { use integer; @a = sort {$a <=> $b} @b },
  ast_sort(
    reverse => 0,
    numeric => 1,
    integer => 1,
    algo => "Merge",
    args => [ast_lexical('@b')],
  ),
  "Standard integer sort"
);

TODO: {
  local $TODO = "inplace sort OP trees make the tree walker barf";
  ast_contains(
    sub { @a = sort @a },
    ast_sort(
      reverse => 1,
      numeric => 0,
      inplace => 1,
      algo => "Merge",
      args => [ast_lexical('@a')],
    ),
    "In-place sort"
  );
}


ast_contains(
  sub { use sort "stable", "_quicksort"; @a = sort @b },
  ast_sort(
    reverse => 0,
    numeric => 0,
    integer => 0,
    algo => "Quick",
    stable => 1,
    args => [ast_lexical('@b')],
  ),
  "Stable quick sort"
);


done_testing();
