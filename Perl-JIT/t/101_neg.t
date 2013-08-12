#!/usr/bin/env perl

use t::lib::Perl::JIT::Test tests => 4;

sub neg {
    my ($a) = @_;

    return -$a;
}

sub neg_and_assign {
    my ($a) = @_;

    $a = -$a;

    return $a;
}

# neg
is_jitting(\&neg, [{ name => 'negate'}]);
is(neg(-42), 42);

# neg and assign
is_jitting(\&neg_and_assign, [{ name => 'negate'}]);
is(neg_and_assign(42), -42);
