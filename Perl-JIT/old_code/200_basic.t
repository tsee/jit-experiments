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


# Testing the int() function
_run_test(
  code => 'my $a = TMPL; my $x = int($a);',
  name => 'int(TMPL)',
  data => [
    [0 => 0],
    [1 => 1],
    [-1 => -1],
    [1.9 => 1],
    [-1.9 => -1],
    [0.9 => 0],
    [-0.9 => 0],
  ]
);

# Testing left shift operator
_run_test(
  code => 'my $a = TMPL; my $b = TMPL; my $x = $a << $b;',
  name => 'TMPL << TMPL',
  data => [
    [1, 0 => 1],
    [1, 1 => 2],
    [1, 2 => 4],
    [2, 2 => 8],
    #[-1, 3, => (-1 << 3)],
  ]
);

# Testing right shift operator
_run_test(
  code => 'my $a = TMPL; my $b = TMPL; my $x = $a >> $b;',
  name => 'TMPL >> TMPL',
  data => [
    [1, 0 => 1],
    [2, 1 => 1],
    [2, 4 => 0],
    #[-1, 4 => (-1 >> 4)],
  ],
);

# FIXME not implemented - not same as perl
# Testing bitwise not ~
#_run_test(
#  code => 'my $a = TMPL; my $x = ~$a;',
#  name => '~TMPL',
#  data => [
#    [1 => ~1],
#    [2 => ~2],
#    [-1 => ~(-1)],
#    [-100 => ~(-100)],
#  ],
#);

sub _run_test {
  my %args = @_;
  my $data = $args{data};

  foreach (@$data) {
    my @d = @$_;
    my $out = pop @d;
    my $code = $args{code};
    $code =~ s/TMPL/$_/ for @d;
    my $name = $args{name};
    $name =~ s/TMPL/$_/ for @d;

    runperl_output_like(
      [qw(-MO=Concise -MPerl::JIT -e), $code],
      qr/\bjitop\[/,
      "'$name' is JIT'd"
    );

    runperl_output_like(
      [qw(-MPerl::JIT -e), $code . ' print "TEST_OUTPUT: $x\n";'],
      qr/^TEST_OUTPUT: \Q$out\E/m,
      "'$name' is JIT'd correctly"
    );
  }
}

done_testing();
