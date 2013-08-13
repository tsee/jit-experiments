#!/usr/bin/env perl

use t::lib::Perl::JIT::Test tests => 2;

sub id {
    my ($a) = @_;

    return $a;
}

sub neg {
    my ($a) = @_;

    return -id($a);
}

# neg
is_jitting(\&neg, [{ name => 'negate'}]);
is(neg(-42), 42);
