#!/usr/bin/env perl

use t::lib::Perl::JIT::Test tests => 6;

sub inc {
    my ($a) = @_;

    return $a + 1;
}

sub inc_assign {
    my ($a) = @_;

    $a += 1;

    return $a;
}

sub inc_and_assign {
    my ($a) = @_;
    my $x = $a + 1;

    return $x;
}

# add
is_jitting(\&inc, [{ name => 'add'}]);
is(inc(41), 42);

# add-assign
is_jitting(\&inc_assign, [{ name => 'add'}]);
is(inc_assign(41), 42);

# add and assign
is_jitting(\&inc_and_assign, [{ name => 'add'}]);
is(inc_and_assign(41), 42);
