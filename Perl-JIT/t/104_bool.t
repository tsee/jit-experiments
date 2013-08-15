#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

my %ops = map {$_ => { name => $_ }} qw(
  not
);
my @tests = (
  { name   => 'boolean not false to true',
    func   => build_jit_test_sub('$a', 'my $x = 1 + !$a;', '$x'),
    opgrep => [$ops{not}],
    output => 2,
    input  => [0], },
  { name   => 'boolean not true to false',
    func   => build_jit_test_sub('$a', 'my $x = 1 + !$a;', '$x'),
    opgrep => [$ops{not}],
    output => 1,
    input  => [1], },
);

# save typing
$_->{output} ||= 42 for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);

