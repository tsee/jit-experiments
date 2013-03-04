use strict;
use warnings;
use Test::More;
use File::Spec;
use lib File::Spec->catdir('t', 'lib'), 'lib';
use Perl::JIT::Test;

runperl_output_like(
  [qw(-MO=Concise -MPerl::JIT -e), 'my $a = 3; my $x = $a + 2'],
  qr/\bjitop\[/,
  "Simple addition is JIT'd"
);

runperl_output_like(
  [qw(-MPerl::JIT -e), 'my $a = 3; my $x = $a + 2; print "TEST_OUTPUT: $x\n";'],
  qr/^TEST_OUTPUT: 5/m,
  "Simple addition is JIT'd correctly"
);

done_testing();
