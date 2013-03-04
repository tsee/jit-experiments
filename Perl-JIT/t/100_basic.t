use strict;
use warnings;
use Test::More;
use File::Spec;
use lib File::Spec->catdir('t', 'lib'), 'lib';
use Perl::JIT::Test;

SCOPE: {
  my $name = "Simple addition is JIT'd";
  my $test_code = 'my $a = 3; my $x = $a + 2; print "TEST_OUTPUT: $x\n";';

  runperl_output_like(
    [qw(-MO=Concise -MPerl::JIT -e), $test_code],
    qr/\bjitop\[/,
    $name
  );

  $name .= ' correctly';
  runperl_output_like(
    [qw(-MPerl::JIT -e), $test_code],
    qr/^TEST_OUTPUT: 5/m,
    $name
  );
}

SCOPE: {
  my $name = "Some arithmetic is JIT'd";
  my $test_code = 'my $a = 3; my $b = 5; my $x = (sin($a)*sin($a) + cos($a)*cos($a)) ** 3 / $b - $a; print "TEST_OUTPUT: $x\n";';
  my $output = runperl_output(
    [qw(-MO=Concise -MPerl::JIT -e), $test_code],
    $name
  );
  like($output, qr/\bjitop\[/, $name);
  like($output, qr/^\S(\s+)<@> jitop.*\n(?:\S\1   <0> padsv\[\$a.*\].*\n){4}\S\1   <0> padsv\[\$b.*\].*\n\S\1   <0> padsv\[\$a.*\].*\n\S\1</m,
       "$name - right # of PADSVs");

  $name .= ' correctly';
  $output = runperl_output(
    [qw(-MPerl::JIT -e), $test_code],
    $name
  );
  ok($output =~ qr/^TEST_OUTPUT: ([+-]?\d+\.\d+)/m, "$name - output sane-ish")
    or diag("Output was: '$output'\n");
  is_approx($1, 1**3 / 5 - 3, "$name - output quite sane");
}


done_testing();
