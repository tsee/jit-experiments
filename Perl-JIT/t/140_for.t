#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

my @tests = (
  { name   => 'simple for',
    func   => sub {
        my ($a) = @_;
        my $r = 0;

        for (my $i = 0; $i < $a; $i += 1) {
            $r += 2 * $i + 1;
        }

        return $r;
    },
    input  => [41],
    output => 1681, },
);

# save typing
$_->{opgrep} ||= [{ name => 'enterloop' }, { name => 'leaveloop' }] for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);
