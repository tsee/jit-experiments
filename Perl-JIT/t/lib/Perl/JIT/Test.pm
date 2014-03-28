package # hide from PAUSE
  Perl::JIT::Test;
use strict;
use warnings;
use Capture::Tiny qw(capture);

use Config '%Config';
use Exporter 'import';
use Test::More;
use Data::Dumper;
use B;
use B::Utils qw(walkoptree_filtered opgrep);
use Perl::JIT::Emit;

our @EXPORT = qw(
  runperl_output_is
  runperl_output_like
  runperl_output
  approx_eq
  is_approx
  run_ctest
  is_jitting
  is_not_jitting
  is_not_jitted
  run_jit_tests
  count_jit_tests
  build_jit_test_sub
  build_jit_types_test_sub
  concise_dump
  compile_and_test
);

SCOPE: {
  my $in_testdir = not(-d 't');
  my $base_dir;
  my $ctest_dir;
  if ($in_testdir) {
    $base_dir = File::Spec->updir;
    #$USE_VALGRIND = -e File::Spec->catfile(File::Spec->updir, 'USE_VALGRIND');
    #$USE_GDB = -e File::Spec->catfile(File::Spec->updir, 'USE_GDB');
  }
  else {
    $base_dir = File::Spec->curdir;
    #$USE_VALGRIND = -e 'USE_VALGRIND';
    #$USE_GDB = -e 'USE_GDB';
  }
  $ctest_dir = File::Spec->catdir($base_dir, 'ctest');

  my @ctests = glob( "$ctest_dir/*.c" );
  my @exe = grep -f $_, map {s/\.c$/$Config{exe_ext}/; $_} @ctests;

  sub _locate_exe {
    my $exe = shift;
    return $exe if -f $exe;
    my $inctest = File::Spec->catfile($ctest_dir, $exe);
    return $inctest if -f $inctest;
    return;
  }

  sub run_ctest {
    my ($executable, $options) = @_;
    my $to_run = _locate_exe($executable);
    return if not defined $to_run;

    #my ($stdout, $stderr) = capture {
      my @cmd;
      #if ($USE_VALGRIND) {
      #  push @cmd, "valgrind", "--suppressions=" .  File::Spec->catfile($base_dir, 'perl.supp');
      #}
      #elsif ($USE_GDB) {
      #  push @cmd, "gdb";
      #}
      push @cmd, $to_run, ref($options)?@$options:();
      note("@cmd");
      system(@cmd)
        and fail("C test did not exit with 0");
    #};
    #print $stdout;
    #warn $stderr if defined $stderr and $stderr ne '';
    return 1;
  } # end run_ctest()
} # end SCOPE

sub runperl_output_is {
  my ($cmd, $ref, $name) = @_;
  my $stdout = _runperl_exec_ok($cmd, $name);
  chomp $stdout;
  is($stdout, $ref, $name);
}

sub runperl_output_like {
  my ($cmd, $ref, $name) = @_;
  my $stdout = _runperl_exec_ok($cmd, $name);
  like($stdout, $ref, $name);
}

sub runperl_output {
  my ($cmd, $name) = @_;
  my $stdout = _runperl_exec_ok($cmd, $name);
  return $stdout;
}

sub _runperl_exec_ok {
  my ($cmd, $name) = @_;

  my ($stdout, $stderr, $rc) = _runperl($cmd);
  is($rc, 0, "$name - Return code is 0")
    or diag("Apparently failed to run " . Data::Dumper::Dumper($cmd));
  $stderr =~ s/-e syntax OK\n?//;
  is($stderr, "", "$name - No output to STDERR")
    or diag("Apparently failed to run " . Data::Dumper::Dumper($cmd));

  return $stdout;
}

sub _runperl {
  my ($cmd) = @_;
  my $rc;
  my ($stdout, $stderr) = capture {
    $rc = system($^X, "-Mblib", (ref($cmd) ? @$cmd : $cmd));
  };
  return ($stdout, $stderr, $rc);
}

sub approx_eq {
  my ($t, $ref, $delta) = @_;
  $delta ||= 1e-9;
  return($t + $delta > $ref && $t - $delta < $ref)
}

sub is_approx {
  my ($t, $ref, $name, $delta) = @_;
  ok(approx_eq($t, $ref, $delta), $name)
    or diag("$t appears to be different from $ref");
}

sub _maybe_concise_dump {
  my ($sub, $name) = @_;
  if ($ENV{CONCISE_DUMP}) {
    print "\n\nConcise dump for '$name'\n";
    concise_dump($sub);
  }
}

sub concise_dump {
  my ($sub) = @_;
  require B::Concise;
  B::Concise::reset_sequence();
  my $walker = B::Concise::compile('','',$sub);
  $walker->();
}

sub _count_matches {
  my ($sub, $opgrep_patterns) = @_;
  my @res;

  for my $pat (@$opgrep_patterns) {
    my $matches = 0;

    walkoptree_filtered(
      B::svref_2object($sub)->ROOT,
      sub { opgrep($pat, @_) },
      sub { ++$matches }
    );

    push @res, $matches;
  }

  return @res;
}

sub is_jitting {
  my ($sub, $opgrep_patterns, $diag) = @_;
  local $Test::Builder::Level = $Test::Builder::Level + 1;
  my $prefix = $diag ? "$diag: " : '';
  my @before = _count_matches($sub, $opgrep_patterns);

  for my $i (0..$#$opgrep_patterns) {
    unless ($before[$i]) {
      ok(0, "${prefix}could not find op matching");
      diag(Dumper($opgrep_patterns->[$i]));
      _maybe_concise_dump($sub, "${prefix}expected OP could not be found");
      return 0;
    }
  }

  Perl::JIT::Emit::jit_sub($sub);

  my @after = _count_matches($sub, $opgrep_patterns);
  for my $i (0..$#$opgrep_patterns) {
    if ($after[$i]) {
      ok(0, "${prefix}some op has not been JITted");
      diag(Dumper($opgrep_patterns->[$i]));
      _maybe_concise_dump($sub, "${prefix}OP-to-be-JITted still found ");
      return 0;
    }
  }

  return ok(1, $diag);
}

sub is_not_jitting {
  my ($sub, $opgrep_patterns, $diag) = @_;
  local $Test::Builder::Level = $Test::Builder::Level + 1;
  my $prefix = $diag ? "$diag: " : '';
  my @before = _count_matches($sub, $opgrep_patterns);

  for my $i (0..$#$opgrep_patterns) {
    unless ($before[$i]) {
      ok(0, "${prefix}could not find op matching");
      diag(Dumper($opgrep_patterns->[$i]));
      _maybe_concise_dump($sub, "${prefix}expected OP could not be found");
      return 0;
    }
  }

  Perl::JIT::Emit->jit_sub($sub);

  my @after = _count_matches($sub, $opgrep_patterns);
  for my $i (0..$#$opgrep_patterns) {
    if ($after[$i] != $before[$i]) {
      ok(0, "${prefix}some op has been JITted");
      diag(Dumper($opgrep_patterns->[$i]));
      _maybe_concise_dump($sub, "${prefix}non-JITted op not found ");
      return 0;
    }
  }

  return ok(1, $diag);
}

sub is_not_jitted {
  my ($sub, $opgrep_patterns, $diag) = @_;
  local $Test::Builder::Level = $Test::Builder::Level + 1;
  my $prefix = $diag ? "$diag: " : '';
  my @count = _count_matches($sub, $opgrep_patterns);

  for my $i (0..$#$opgrep_patterns) {
    unless ($count[$i]) {
      ok(0, "${prefix}could not match pattern");
      diag(Dumper($opgrep_patterns->[$i]));
      _maybe_concise_dump($sub, "${prefix}non-JITted op not found ");
      return 0;
    }
  }

  return ok(1, $diag);
}

sub count_jit_tests {
  my $tests = shift;
  return 2 * @$tests;
}

sub run_jit_tests {
  my $tests = shift;

  foreach my $test (@$tests) {
    is_jitting($test->{func}, $test->{opgrep}, $test->{name});

    my $tname = "$test->{name}: Checking output";
    if (ref($test->{output}) and ref($test->{output}) eq 'CODE') {
      ok($test->{output}->( $test->{func}->(@{$test->{input}}) ),
         $tname);
    }
    else {
      is_deeply( $test->{func}->(@{$test->{input}}),
                 $test->{output},
                 $tname);
    }
  }
}

sub build_jit_test_sub {
  my ($params, $code, $retval) = @_;
  my $param_code = defined($params) ? "my ($params) = \@_;" : "";
  my $subcode = qq[
    use Perl::JIT;
    \$sub = sub {
      $param_code

      $code;

      return($retval);
    }
  ];
  my $sub;
  my $ok = eval $subcode;
  if (!$ok) {
    my $err = $@ || 'Zombie error';
    die "Failed to compile test function code:\n$subcode";
  }
  return $sub;
}

sub build_jit_types_test_sub {
  my ($params, $types) = @_;
  my $decl = @$types ? "typed @$types" : "my";
  my $args = join ', ', @$params;
  my $exp  = join ' + ', @$params, 1;
  my $subcode = <<EOT;
  use Perl::JIT;

  sub {
    $decl ($args) = \@_;

    return $exp;
  }
EOT
  my $sub = eval $subcode;
  if (!$sub) {
    my $err = $@ || 'Zombie error';
    die "Failed to compile test function code:\n$subcode";
  }
  return $sub;
}

sub compile_and_test {
  my %args = @_;
  my $code = $args{code} // die;
  my @args = @{ $args{args} || [] };
  my $cmp_fun = $args{cmp_fun} // \&is_approx;
  my $name = $args{name} // "Unnamed test";
  my $repeat = $args{repeat} // 1e6;

  $code = "use Perl::JIT; $code";
  my $perl_sub = eval $code
  or die "Failed to eval code: $@";
  my $jit_sub = eval $code
    or die "Failed to eval code: $@";

  Perl::JIT::Emit->jit_sub($jit_sub);

  my $exp = $perl_sub->(@args);
  my $res = $jit_sub->(@args);
  $cmp_fun->($exp, $res, $name);

  if ($ENV{BENCHMARK}) {
    require Dumbbench;
    my $bench = Dumbbench->new(
      verbosity => ($ENV{BENCHMARK}||1)-1,
      target_rel_precision => 0.01,
      initial_runs         => 20,
    );
    $bench->add_instances(
      Dumbbench::Instance::PerlSub->new(name => "perl", code => sub {$perl_sub->(@args) for 1..$repeat;}),
      Dumbbench::Instance::PerlSub->new(name => "jit",  code => sub {$jit_sub->(@args)  for 1..$repeat;}),
    );

    $bench->run;
    print "\n=======================================\nBenchmarking $name:\n";
    $bench->report;
    print "\n";
  }
}

package t::lib::Perl::JIT::Test;

use strict;
use warnings;
use parent 'Test::Builder::Module';

use Test::More;
use Test::Differences;

Perl::JIT::Test->import;

our @EXPORT = (
  @Test::More::EXPORT,
  @Test::Differences::EXPORT,
  @Perl::JIT::Test::EXPORT,
);

sub import {
    unshift @INC, 't/lib';

    strict->import;
    warnings->import;

    goto &Test::Builder::Module::import;
}

1;
