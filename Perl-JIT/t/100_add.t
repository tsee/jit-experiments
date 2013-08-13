#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;

sub inc {
    my ($a) = @_;

    return $a + 1;
}

sub inc_assign {
    my ($a) = @_;

    $a += 1;

    return $a;
}

sub add_assign {
    my ($a) = @_;

    $a += $a + 2;

    return $a;
}

sub inc_and_assign {
    my ($a) = @_;
    my $x = $a + 1;

    return $x;
}

sub add_and_assign {
    my ($a, $b) = @_;

    # declaration in separate statement
    my $x;
    $x = $a + $b;

    return $x;
}

sub add_and_assign_declare {
    my ($a, $b) = @_;

    # declaration in same statement (OP flags differ from separate decl)
    my $x = $a + $b;

    return $x;
}

sub add_nested {
    my ($a, $b) = @_;

    my $x = abs(abs($a) + abs($b));

    return $x;
}

my @tests = (
  { name   => 'add constant',
    func   => \&inc,
    input  => [41], },
  { name   => 'add-assign constant',
    func   => \&inc_assign,
    input  => [41], },
  { name   => 'add-assign with non-constant',
    func   => \&add_assign,
    input  => [20], },
  { name   => 'add and assign with non-constant',
    func   => \&inc_and_assign,
    input  => [41], },
  { name   => 'add variables, no declaration',
    func   => \&add_and_assign,
    input  => [41, 1], },
  { name   => 'add variables, with declaration',
    func   => \&add_and_assign_declare,
    input  => [37, 5], },
  # TODO unjitted subtrees not supported yet
  #{ name   => 'add op nested in other ops',
  #  func   => \&add_nested,
  #  input  => [38, 4], },
);

plan tests => 2 * scalar(@tests);

my $opgrep = [{ name => 'add' }];
my $output = 42;
foreach my $test (@tests) {
  $test->{opgrep} = $opgrep; # shortcut
  $test->{output} = $output;

  is_jitting($test->{func}, $test->{opgrep}, $test->{name});

  is_deeply( $test->{func}->(@{$test->{input}}),
             $test->{output},
             "$test->{name}: Checking output");
}

