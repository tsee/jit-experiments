#!perl
use strict;
use warnings;
use Test::More;
use Carp qw(croak);

use t::lib::Perl::JIT::Test;

compile_and_test(
  code => q{
    sub { # iteration
      typed Double $x = 0.0;
      for (typed Int $i = 1; $i < 1e5; ++$i) {
        $x += $i + 1./$i;
      }
      return $x;
    }
  },
  args => [],
  name => "iteration and arithmetic",
  repeat => 20,
  cmp_fun => sub {
    ok($_[0] < $_[1]+1e-5 && $_[0] > $_[1]-1e-5, $_[2]);
  },
);

fail("'while and rand' segfaults");
#compile_and_test(
#  code => q{
#    sub { # while and rand
#      typed Double $x = 0.0;
#      srand(0);
#      while ($x < 1000.) {
#        $x += rand();
#      }
#      return $x;
#    }
#  },
#  args => [],
#  name => "while and rand",
#  repeat => 1e3,
#  cmp_fun => sub {
#    ok($_[0] < $_[1]+1e-4 && $_[0] > $_[1]-1e-4, $_[2]);
#  },
#);

done_testing();
