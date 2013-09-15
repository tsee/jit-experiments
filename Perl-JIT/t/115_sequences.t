#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

my @tests = (
  { name   => 'addition sequence',
    func   => build_jit_test_sub('$x', '$x += 30; $x += 5', '$x'),
    opgrep => [{ name => 'nextstate', sibling => { name => 'add' } }],
    input  => [7], },
  { name   => 'mixed jittable/non-jittable',
    func   => build_jit_test_sub('$x', '$x += 30; srand(1); $x += 5', '$x'),
    opgrep => [{ name => 'nextstate', sibling => { name => 'add' } }],
    input  => [7], },
  { name   => 'mixed jittable/non-jittable',
    func   => build_jit_test_sub('$x', 'for (1..35) { $x += 1 }', '$x'),
    opgrep => [{ name => 'nextstate', sibling => { name => 'add' } }],
    input  => [7], },
);

# save typing
$_->{output} = 42 for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);
