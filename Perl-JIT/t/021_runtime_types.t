#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;
use Perl::JIT qw(:all);
use Perl::JIT::Types;

my @tests = (
  # basic types
  { name => 'unspecified',
    vars => ['$x', '$y'],
    type => {'' => {class => 'Scalar', tag => pj_unspecified_type}},
    },
  { name => 'int',
    vars => ['$x', '$y'],
    type => {'Int' => {class => 'Scalar', tag => pj_int_type}},
    },
  # types mixed with other attributes
  { name => 'other_attributes',
    vars => ['$x', '$y'],
    type => {''    => {class => 'Scalar', tag => pj_unspecified_type},
             'Bar' => undef},
    },
  { name => 'mixed',
    vars => ['$x', '$y'],
    type => {'Int' => {class => 'Scalar', tag => pj_int_type},
             'Bar' => undef},
    },
);

plan tests => 6 * @tests;

my $bar_attributes_processed = 0;

sub MODIFY_SCALAR_ATTRIBUTES {
  my ($package, $ref, @attrs) = @_;
  my @rest = grep $_ ne 'Bar', @attrs;

  $bar_attributes_processed += @attrs - @rest;

  return @rest;
}

for my $test (@tests) {
  my @types = grep $_, keys %{$test->{type}};
  my @values = (grep $_, values %{$test->{type}}) x @{$test->{vars}};
  my $sub = build_jit_types_test_sub($test->{vars}, \@types);

  my $expected_modify = (grep $_ eq 'Bar', @types) * @{$test->{vars}};

  $bar_attributes_processed = 0;
  is($sub->(2, 1), 4, "$test->{name}: sanity check");
  is($bar_attributes_processed, $expected_modify,
     "$test->{name}: MODIFY calls");

  # check that the sub still works
  $bar_attributes_processed = 0;
  is($sub->(2, 1), 4, "$test->{name}: double check");
  is($bar_attributes_processed, $expected_modify,
     "$test->{name}: MODIFY calls double check");

  # check that the JIT can still find the attributes optree
  my @asts = Perl::JIT::find_jit_candidates($sub);
  my @vars;

  while (@asts) {
    my $ast = pop @asts;
    push @asts, $ast->get_kids;
    if ($ast->get_type == pj_ttype_variable) {
        my $type = $ast->get_value_type;
        my $cls = ref($type) =~ s/.*:://r;
        push @vars, {class => $cls, tag => $type ? $type->tag : -1};
    }
  }

  is(scalar @vars, scalar @{$test->{vars}}, "$test->{name}: variable count matches");
  eq_or_diff(\@vars, \@values, "$test->{name}: variable types match");
}
