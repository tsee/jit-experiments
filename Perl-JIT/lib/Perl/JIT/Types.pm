package Perl::JIT::Types;

use strict;
use warnings;

use Exporter 'import';

use Perl::JIT qw(:all);
use attributes;

our $OLD_IMPORT = \&attributes::import;

SCOPE: {
  no warnings 'redefine';
  *attributes::import = \&replace_attributes_import;
}

use constant {
    SCALAR         => Perl::JIT::AST::Scalar->new(pj_scalar_type),
    DOUBLE         => Perl::JIT::AST::Scalar->new(pj_double_type),
    INT            => Perl::JIT::AST::Scalar->new(pj_int_type),
    UNSIGNED_INT   => Perl::JIT::AST::Scalar->new(pj_uint_type),
    UNSPECIFIED    => Perl::JIT::AST::Scalar->new(pj_unspecified_type),
};

our @EXPORT_OK = qw(SCALAR DOUBLE INT UNSIGNED_INT UNSPECIFIED);
our %EXPORT_TAGS = (
  all           => \@EXPORT_OK,
);

1;
