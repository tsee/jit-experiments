#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

my @tests = (
  { name   => 'assign to declaration',
    func   => build_jit_test_sub('$x', 'my $y = $x', '$y + 1'),
    opgrep => [{ name => 'sassign', first => { name => 'padsv' } },
               { name => 'add' }],
    input  => [41], },
  { name   => 'assign expression to declaration',
    func   => build_jit_test_sub('$x', 'my $y = $x + 1', '$y'),
    opgrep => [{ name => 'sassign', first => { name => 'add' } },
               { name => 'add' }],
    input  => [41], },
);

# save typing
$_->{output} = 42 for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);
