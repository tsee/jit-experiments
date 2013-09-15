#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

my @tests = (
  { name   => 'addition sequence',
    func   => build_jit_test_sub('$x', '$x += 30; $x += 5', '$x'),
    opgrep => [{ name => 'nextstate', sibling => { name => 'add' } }],
    input  => [7], },
);

# save typing
$_->{output} = 42 for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);
