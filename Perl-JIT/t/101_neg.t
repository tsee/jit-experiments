#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

my @tests = (
  { name   => 'negate',
    func   => build_jit_test_sub('$a', '', '-$a'),
    input  => [-42], },
  { name   => 'negate and assign',
    func   => build_jit_test_sub('$a', '$a = -$a;', '$a'),
    input  => [-42], },
  { name   => 'negate, declare, and assign',
    func   => build_jit_test_sub('$a', 'my $x = -$a;', '$x'),
    input  => [-42], },
  { name   => 'negate, add, and assign',
    func   => build_jit_test_sub('$a', '$a = $a + -$a + -$a;', '$a'),
    input  => [-42], },
);

# save typing
$_->{output} = 42 for @tests;
$_->{opgrep} = [{ name => 'negate' }] for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);
