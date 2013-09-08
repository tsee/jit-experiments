package Perl::JIT::Types;

use strict;
use warnings;

use Exporter 'import';

use Perl::JIT qw(:all);

use constant {
    OPAQUE         => Perl::JIT::AST::Scalar->new(pj_opaque_type),
    DOUBLE         => Perl::JIT::AST::Scalar->new(pj_double_type),
    INT            => Perl::JIT::AST::Scalar->new(pj_int_type),
    UNSIGNED_INT   => Perl::JIT::AST::Scalar->new(pj_uint_type),
    UNSPECIFIED    => Perl::JIT::AST::Scalar->new(pj_unspecified_type),
    # used in code generation
    ANY            => Perl::JIT::AST::Scalar->new(pj_any_type),
    SCALAR         => Perl::JIT::AST::Scalar->new(pj_scalar_type),
    GV             => Perl::JIT::AST::Scalar->new(pj_gv_type),
};

our @EXPORT_OK = qw(
    OPAQUE DOUBLE INT UNSIGNED_INT UNSPECIFIED ANY SCALAR GV
    minimal_covering_type
);
our %EXPORT_TAGS = (
    all => \@EXPORT_OK,
);

1;
