package # hide from PAUSE
  Perl::JIT::Test;
use strict;
use warnings;
use Capture::Tiny qw(capture);

use Exporter 'import';
use Test::More;

our @EXPORT = qw(
  runperl_output_is
  runperl_output_like
  runperl_output
  is_approx
);

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
  is($rc, 0, "$name - Return code is 0");
  $stderr =~ s/-e syntax OK\n?//;
  is($stderr, "", "$name - No output to STDERR");

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

sub is_approx {
  my ($t, $ref, $name) = @_;
  ok($t + 1e-9 > $ref && $t - 1e9 < $ref, $name)
    or diag("$t appears to be different from $ref");
}

1;
