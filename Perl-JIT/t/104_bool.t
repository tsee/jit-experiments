#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

my %ops = map {$_ => { name => $_ }} qw(
  not and or cond_expr
);

# FIXME the 1+ prefixes in the tests are to protect the scalar assignments from
#       the ANY return types of some of the logops.
my @tests = (
  { name   => 'wrapped: boolean not false to true',
    func   => build_jit_test_sub('$a', 'my $x = 1 + !$a;', '$x'),
    opgrep => [$ops{not}],
    output => 2,
    input  => [0], },
  { name   => 'wrapped: boolean not true to false',
    func   => build_jit_test_sub('$a', 'my $x = 1 + !$a;', '$x'),
    opgrep => [$ops{not}],
    output => 1,
    input  => [1], },
  { name   => 'boolean not false to true',
    func   => build_jit_test_sub('$a', 'my $x = !$a;', '$x'),
    opgrep => [$ops{not}],
    output => 1,
    input  => [0], },
  { name   => 'boolean not true to false',
    func   => build_jit_test_sub('$a', 'my $x = !$a;', '$x'),
    opgrep => [$ops{not}],
    output => 0,
    input  => [1], },

  { name   => 'wrapped-boolean and: yes with types',
    func   => build_jit_test_sub(undef, 'typed Int ($a, $b)=@_; my $x = 1+($a && $b);', '$x'),
    opgrep => [$ops{and}],
    output => 1000,
    input  => [99, 999], },
  { name   => 'wrapped-boolean and: right no',
    func   => build_jit_test_sub('$a, $b', 'my $x = 1+($a && $b);', '$x'),
    opgrep => [$ops{and}],
    output => 1,
    input  => [2, 0], },
  { name   => 'boolean and: yes',
    func   => build_jit_test_sub('$a, $b', 'my $x = ($a && $b);', '$x'),
    opgrep => [$ops{and}],
    output => 999,
    input  => [99, 999], },
  { name   => 'boolean and: left no',
    func   => build_jit_test_sub('$a, $b', 'my $x = ($a and $b);', '$x'),
    opgrep => [$ops{and}],
    output => 0,
    input  => [0, 9999], },
  { name   => 'boolean and: right no',
    func   => build_jit_test_sub('$a, $b', 'my $x = ($a && $b);', '$x'),
    opgrep => [$ops{and}],
    output => 0,
    input  => [2, 0], },

  { name   => 'wrapped-boolean or: right with types',
    func   => build_jit_test_sub(undef, 'typed Int ($a, $b)=@_; my $x = 1+($a || $b);', '$x'),
    opgrep => [$ops{or}],
    output => 1000,
    input  => [0, 999], },
  { name   => 'wrapped-boolean or: left',
    func   => build_jit_test_sub('$a, $b', 'my $x = 1+($a or $b);', '$x'),
    opgrep => [$ops{or}],
    output => 3,
    input  => [2, 42], },
  { name   => 'boolean or: right',
    func   => build_jit_test_sub('$a, $b', 'my $x = ($a || $b);', '$x'),
    opgrep => [$ops{or}],
    output => 999,
    input  => [0, 999], },
  { name   => 'boolean or: left',
    func   => build_jit_test_sub('$a, $b', 'my $x = ($a || $b);', '$x'),
    opgrep => [$ops{or}],
    output => 2,
    input  => [2, 42], },
  { name   => 'boolean or: no',
    func   => build_jit_test_sub('$a, $b', 'my $x = ($a || $b);', '$x'),
    opgrep => [$ops{or}],
    output => 0,
    input  => [0, 0], },

  { name   => 'wrapped-ternary: left with types',
    func   => build_jit_test_sub(undef, 'typed Int ($a, $b, $c)=@_; my $x = 1+($a ? $b : $c);', '$x'),
    opgrep => [$ops{cond_expr}],
    output => 3,
    input  => [1, 2, 3], },
  { name   => 'wrapped-ternary: left',
    func   => build_jit_test_sub('$a, $b, $c', 'my $x = 1+($a ? $b : $c);', '$x'),
    opgrep => [$ops{cond_expr}],
    output => 3,
    input  => [1, 2, 3], },
  { name   => 'ternary: left with types',
    func   => build_jit_test_sub(undef, 'typed Int ($a, $b, $c)=@_; my $x = ($a ? $b : $c);', '$x'),
    opgrep => [$ops{cond_expr}],
    output => 2,
    input  => [1, 2, 3], },
  { name   => 'ternary: right',
    func   => build_jit_test_sub('$a, $b, $c', 'my $x = ($a ? $b : $c);', '$x'),
    opgrep => [$ops{cond_expr}],
    output => 3,
    input  => [0, 2, 3], },
  { name   => 'ternary: left',
    func   => build_jit_test_sub('$a, $b, $c', 'my $x = ($a ? $b : $c);', '$x'),
    opgrep => [$ops{cond_expr}],
    output => 2,
    input  => [1, 2, 3], },
);

# save typing
$_->{output} //= 42 for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);

