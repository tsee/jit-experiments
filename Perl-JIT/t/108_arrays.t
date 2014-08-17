#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

my %ops = map {$_ => { name => $_ }} qw(
  aelem aelemfast aelemfast_lex sassign
);
$ops{aelemfast_lex} = $ops{aelemfast} if $] <= 5.014;
my @tests = (
  { name   => 'access array element',
    func   => build_jit_test_sub(undef, 'my $a; my @x = (1,42,2); $a = $x[1]', '$a'),
    opgrep => [@ops{qw(aelemfast_lex sassign)}],
    input  => [], },
  { name   => 'assign array element',
    func   => build_jit_test_sub('$a', 'my @x = (1,2,3); $x[2] = $a; $a = $x[2]', '$a'),
    opgrep => [@ops{qw(aelemfast_lex sassign)}],
    input  => [42], },
  { name   => 'assign and return array element',
    func   => build_jit_test_sub('$a', 'my @x = (1,2,3); $x[2] = $a', '$x[2]'),
    opgrep => [@ops{qw(aelemfast_lex sassign)}],
    input  => [42], },
);

# save typing
$_->{output} //= 42 for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);
