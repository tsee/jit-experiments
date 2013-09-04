#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

my @tests = (
  { name   => 'add constant to global',
    func   => build_jit_test_sub('$x', '$a = $x', '$a + 1'),
    input  => [41], },
  { name   => 'add-assign constant to global',
    func   => build_jit_test_sub('$x', '$a = $x; $a += 1', '$a'),
    input  => [41], },
);

# save typing
$_->{output} = 42 for @tests;
$_->{opgrep} = [{ name => 'add' }] for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);
