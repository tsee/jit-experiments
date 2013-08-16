#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

my %ops = map {$_ => { name => $_ }} qw(
  multiply divide add subtract sin cos
  sqrt log exp int
);
my @tests = (
  { name   => 'multiply identity',
    func   => build_jit_test_sub('$a', '', '1.0 * $a'),
    opgrep => [$ops{multiply}],
    input  => [42], },
  { name   => 'multiply constant',
    func   => build_jit_test_sub('$a', '', '$a * 2'),
    opgrep => [$ops{multiply}],
    input  => [21], },
  { name   => 'multiply & divide constant',
    func   => build_jit_test_sub('$a', '', '2.0*($a/2.0)'),
    opgrep => [@ops{qw(multiply divide)}],
    output => sub {approx_eq($_[0], 42)},
    input  => [42], },
  { name   => 'multiply & subtract vars',
    func   => build_jit_test_sub('$a, $b, $c', 'my $x = $a*$b - $c - 1', '$x+1'),
    opgrep => [@ops{qw(multiply subtract)}],
    output => sub {approx_eq($_[0], 42)},
    input  => [21, 3, 21], },
  { name   => 'cos(sin())',
    func   => build_jit_test_sub('$a', '', 'cos(sin($a))'),
    opgrep => [@ops{qw(sin cos)}],
    output => sub {approx_eq($_[0], cos(sin(2)))},
    input  => [2], },
  { name   => 'Complex expression',
    func   => build_jit_test_sub('$a, $b', 'my $x = -sin($a) * -2*cos($b) - cos($a)/sin($b) +1', '$x'),
    opgrep => [@ops{qw(sin cos)}],
    output => sub { approx_eq($_[0], -sin(1)*-2*cos(2)-cos(1)/sin(2)+1, 1e-6) },
    input  => [1,2], },
  { name   => 'log(exp())',
    func   => build_jit_test_sub('$a', '', 'log(exp($a))'),
    opgrep => [@ops{qw(log exp)}],
    output => sub {approx_eq($_[0], log(exp(20)), 1e-6)},
    input  => [20], },
  { name   => 'sqrt(exp())',
    func   => build_jit_test_sub('$a, $b', '', '$b + sqrt(exp($a))'),
    opgrep => [@ops{qw(sqrt exp add)}],
    output => sub {approx_eq($_[0], 2 + sqrt(exp(3)), 1e-6)},
    input  => [3, 2], },
  { name   => 'int(42.1)',
    func   => build_jit_test_sub('$a', '', 'int($a)'),
    opgrep => [@ops{qw(int)}],
    input  => [42.1], },
);

# save typing
$_->{output} ||= 42 for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);

