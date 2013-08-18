#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;
use Perl::JIT qw(:all);

my @tests = (
  # basic types
  { name => 'unspecified',
    vars => ['$x'],
    type => {'' => {class => 'Scalar', tag => pj_unspecified_type}},
    },
  { name => 'scalar',
    vars => ['$x', '$y'],
    type => {'Scalar' => {class => 'Scalar', tag => pj_scalar_type}},
    },
  { name => 'string',
    vars => ['$x'],
    type => {'String' => {class => 'Scalar', tag => pj_string_type}},
    },
  { name => 'double',
    vars => ['$x', '$y'],
    type => {'Double' => {class => 'Scalar', tag => pj_double_type}},
    },
  { name => 'int',
    vars => ['$x'],
    type => {'Int' => {class => 'Scalar', tag => pj_int_type}},
    },
  { name => 'unsigned int',
    vars => ['$x', '$y'],
    type => {'UnsignedInt' => {class => 'Scalar', tag => pj_uint_type}},
    },
  # types mixed with other attributes
  { name => 'other_attributes',
    vars => ['$x'],
    type => {''    => {class => 'Scalar', tag => pj_unspecified_type},
             'Bar' => undef},
    },
  { name => 'mixed',
    vars => ['$x'],
    type => {'Int' => {class => 'Scalar', tag => pj_int_type},
             'Bar' => undef},
    },
);

plan tests => 2 * @tests;

for my $test (@tests) {
  my @types = grep $_, keys %{$test->{type}};
  my @values = (grep $_, values %{$test->{type}}) x @{$test->{vars}};
  my $sub = build_jit_types_test_sub($test->{vars}, \@types);
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
