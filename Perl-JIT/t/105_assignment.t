#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

# Makes the assumption that srand() is never going to be JITted

my @tests = (
  { name   => 'assign constant',
    func   => build_jit_test_sub('$a', '$a = 42', '$a'),
    input  => [41], },
  { name   => 'assign variable',
    func   => build_jit_test_sub('$a', 'my $x; $x = 42; $a = $x', '$a'),
    input  => [41], },
  { name   => 'assign add-assignment',
    func   => build_jit_test_sub('$a', 'my $x; $x = $a += 1', '$x'),
    input  => [41], },
  { name   => 'assign expression',
    func   => build_jit_test_sub('$a', 'my $x; $x = $a; $a = abs($x)', '$a'),
    input  => [-42], },
  { name   => 'assign in scalar context',
    func   => build_jit_test_sub('$a', 'my $y; my $z = srand($y = $a)', '$z + $y'),
    input  => [21], },
  { name   => 'assign array element',
    func   => build_jit_test_sub('$a', 'my @x; $x[0] = abs($a)', '$x[0]'),
    input  => [-42], },
  { name   => 'assign ternary',
    func   => build_jit_test_sub('$a', 'my ($x, $y, $z) = 1; ($x ? $y : $z) = $a', '$y'),
    input  => [42], },
);

# save typing
$_->{output} //= 42 for @tests;
$_->{opgrep} ||= [{ name => 'sassign' }] for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);
