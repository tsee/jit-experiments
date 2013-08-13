#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

my $mult_op = { name => 'multiply' };
my $div_op = { name => 'divide' };
my $sub_op = { name => 'subtract' };
my @tests = (
  { name   => 'multiply identity',
    func   => build_jit_test_sub('$a', '', '1.0 * $a'),
    opgrep => [$mult_op],
    input  => [42], },
  { name   => 'multiply constant',
    func   => build_jit_test_sub('$a', '', '$a * 2'),
    opgrep => [$mult_op],
    input  => [21], },
  { name   => 'multiply & divide constant',
    func   => build_jit_test_sub('$a', '', '2.0*($a/2.0)'),
    opgrep => [$mult_op, $div_op],
    output => sub {approx_eq($_[0], 42)},
    input  => [42], },
  { name   => 'multiply & subtract vars',
    func   => build_jit_test_sub('$a, $b, $c', 'my $x = $a*$b - $c - 1', '$x+1'),
    opgrep => [$mult_op, $sub_op],
    output => sub {approx_eq($_[0], 42)},
    input  => [21, 3, 21], },
);

# save typing
$_->{output} ||= 42 for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);

