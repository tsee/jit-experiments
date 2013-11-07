#!perl
use strict;
use warnings;
use Test::More;
use Carp qw(croak);

use t::lib::Perl::JIT::Test;

my $code = <<'HERE';
sub { # vector_dot_product
  my ($v1, $v2) = @_;
  croak("Vectors don't have same dimensionality")
    if not @$v1 == @$v2;

  typed Int $i;
  typed Int $n = @$v1;
  typed Double $prod = 0.0;

  for ($i = 0; $i < $n; ++$i) {
    $prod += $v1->[$i] * $v2->[$i];
  }

  return $prod;
}
HERE

my $v1 = [2.5,4.1,-5.1,2.1,1.0, 1..100];
my $v2 = [reverse(1..100), 30.3,-20.1, 1., 4.1, -12.1];

compile_and_test(
  code => $code,
  args => [$v1, $v2],
  name => "vector dot product",
  repeat => 2e4,
);

done_testing();
