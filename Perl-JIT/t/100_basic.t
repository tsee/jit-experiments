use strict;
use warnings;
use Test::More;
use File::Spec;
use lib File::Spec->catdir('t', 'lib'), 'lib';
use Perl::JIT::Test;

runperl_output_like(
  [qw(-MO=Concise -MPerl::JIT -e),
    'my $a = 3; my $x = $a + 2; print "TEST_OUTPUT: $x\n";'],
  qr/\bjitop\[/,
  "Simple addition is JIT'd"
);

runperl_output_like(
  [qw(-MPerl::JIT -e),
    'my $a = 3; my $x = $a + 2; print "TEST_OUTPUT: $x\n";'],
  qr/^TEST_OUTPUT: 5/m,
  "Simple addition is JIT'd correctly"
);

SCOPE: {
  my $name = "Some arithmetic is JIT'd";
  my $output = runperl_output(
    [qw(-MO=Concise -MPerl::JIT -e),
      'my $a = 3; my $b = 5; my $x = (sin($a)*sin($a) + cos($a)*cos($a)) ** 3 / $b - $a; print "TEST_OUTPUT: $x\n";'],
    $name
  );
  like($output, qr/\bjitop\[/, $name);

  $name .= ' correctly';
  $output = runperl_output(
    [qw(-MPerl::JIT -e),
      'my $a = 3; my $b = 5; my $x = (sin($a)*sin($a) + cos($a)*cos($a)) ** 3 / $b - $a; print "TEST_OUTPUT: $x\n";'],
    $name
  );
  ok($output =~ qr/^TEST_OUTPUT: ([+-]?\d+\.\d+)/m, "$name - output sane-ish")
    or diag("Output was: '$output'\n");
  is_approx($1, 1**3 / 5 - 3, "$name - output quite sane");
}



done_testing();
