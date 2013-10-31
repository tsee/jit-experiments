#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

my @tests = (
  { name   => 'simple while',
    func   => sub {
        my ($a) = @_;
        my $r = 0;

        while ($a >= 0) {
            $r += $a;
            $a -= 1;
        }

        return $r;
    },
    input  => [41],
    output => 861, },
  { name   => 'simple until',
    func   => sub {
        my ($a) = @_;
        my $r = 0;

        until ($a < 5) {
            $r += $a;
            $a -= 1;
        }

        return $r;
    },
    input  => [41],
    output => 851, },
);

# save typing
$_->{opgrep} ||= [{ name => 'enterloop' }, { name => 'leaveloop' }] for @tests;

plan tests => count_jit_tests(\@tests);

run_jit_tests(\@tests);
