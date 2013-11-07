#!perl
use strict;
use warnings;
use Test::More;
use Carp qw(croak);

use t::lib::Perl::JIT::Test;

my $code = <<'HERE';
sub { # iteration
  typed Double $x = 0.0;
  for (typed Int $i = 1; $i < 2e6; ++$i) {
    $x += $i + 1./$i;
  }
  return $x;
}
HERE

compile_and_test(
  code => $code,
  args => [],
  name => "iteration and arithmetic",
  repeat => 1,
  cmp_fun => sub {
    ok($_[0] < $_[1]+1e-5 && $_[0] > $_[1]-1e-5, $_[2]);
  },
);

done_testing();
