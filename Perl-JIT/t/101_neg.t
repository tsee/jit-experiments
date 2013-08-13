#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

sub neg {
    my ($a) = @_;

    return -$a;
}

sub neg_and_assign {
    my ($a) = @_;

    $a = -$a;

    return $a;
}

sub neg_and_assign_declare {
    my ($a) = @_;

    my $x = -$a;

    return $x;
}

sub neg_add_assign {
    my ($a) = @_;

    $a = $a + -$a + -$a;

    return $a;
}

my @tests = (
  { name   => 'negate',
    func   => \&neg,
    input  => [-42], },
  { name   => 'negate and assign',
    func   => \&neg_and_assign,
    input  => [-42], },
  { name   => 'negate, declare, and assign',
    func   => \&neg_and_assign_declare,
    input  => [-42], },
  { name   => 'negate, add, and assign',
    func   => \&neg_add_assign,
    input  => [-42], },
);

# save typing
$_->{output} = 42 for @tests;
$_->{opgrep} = [{ name => 'negate' }] for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);
