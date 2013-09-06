#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

my @tests = (
  { name   => 'add constant',
    func   => build_jit_test_sub('$a', '', '$a + 1'),
    input  => [41], },
  { name   => 'add-assign constant',
    func   => build_jit_test_sub('$a', '$a += 1', '$a'),
    input  => [41], },
  { name   => 'add-assign with non-constant',
    func   => build_jit_test_sub('$a', '$a += $a + 2', '$a'),
    input  => [20], },
  { name   => 'add and assign with non-constant',
    func   => build_jit_test_sub('$a', 'my $x = $a + 1', '$x'),
    input  => [41], },
  { name   => 'add variables, no declaration',
    func   => build_jit_test_sub('$a, $b', 'my $x; $x = $a + $b;', '$x'),
    input  => [41, 1], },
  { name   => 'add variables, with declaration',
    func   => build_jit_test_sub('$a, $b', 'my $x = $a + $b;', '$x'),
    input  => [37, 5], },
  { name   => 'add op nested in other ops',
    func   => build_jit_test_sub('$a, $b', 'my $x = abs(abs($a) + abs($b));', '$x'),
    input  => [38, 4], },
);

# save typing
$_->{output} = 42 for @tests;
$_->{opgrep} = [{ name => 'add' }] for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);

