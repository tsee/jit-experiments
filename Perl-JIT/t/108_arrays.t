#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

sub assign_elem    { $_[0] = $_[1] }
sub no_assign_elem { $_[0] }

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
  { name   => 'assign and return array element',
    func   => build_jit_test_sub('$a', 'my @x = (1,2,3); $x[2] = $a', '$x[2]'),
    opgrep => [@ops{qw(aelemfast_lex sassign)}],
    input  => [42], },
  { name   => 'assign and return array element with index expression',
    func   => build_jit_test_sub('$a', 'my @x = (1,2,3); my $t = 2; $x[$t] = $a', '$x[$t]'),
    opgrep => [@ops{qw(aelem sassign)}],
    input  => [42], },
  { name   => 'localize existing array element',
    func   => build_jit_test_sub('$a', 'my @x = (1,2,$a); { local $x[2]; $x[2] = 2; }', '$x[2]'),
    opgrep => [@ops{qw(aelem aelemfast_lex sassign)}],
    input  => [42], },
  { name   => 'localize missing array element',
    func   => build_jit_test_sub('$a', 'my @x = (1,2,$a); { local $x[7]; $x[3] = 2; }', '$#x'),
    opgrep => [@ops{qw(aelem aelemfast_lex sassign)}],
    output => 3,
    input  => [42], },
  { name   => 'element is sub call - not assigned',
    func   => build_jit_test_sub('$a', 'my @x; main::no_assign_elem($x[7], 2);', '$#x'),
    opgrep => [@ops{qw(aelem)}],
    output => -1,
    input  => [], },
  { name   => 'element is sub call - assigned',
    func   => build_jit_test_sub('$a', 'my @x; main::assign_elem($x[7], 2);', '$#x + $x[7]'),
    opgrep => [@ops{qw(aelem aelemfast_lex)}],
    output => 9,
    input  => [], },
);

# save typing
$_->{output} //= 42 for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);
