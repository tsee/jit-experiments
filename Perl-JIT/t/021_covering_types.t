#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;
use Perl::JIT qw(:all);
use Perl::JIT::Types qw(:all);

my $t;

$t = minimal_covering_type([]);
ok(!defined($t));

my $str_type = Perl::JIT::AST::Scalar->new(pj_string_type);
my $ary_uint_type = Perl::JIT::AST::Array->new(UNSIGNED_INT);
my $hash_uint_type = Perl::JIT::AST::Hash->new(UNSIGNED_INT);

my $ary_hash_double_type = Perl::JIT::AST::Array->new(Perl::JIT::AST::Hash->new(DOUBLE));
my $ary_hash_scalar_type = Perl::JIT::AST::Array->new(Perl::JIT::AST::Hash->new(SCALAR));
my $ary_hash_int_type    = Perl::JIT::AST::Array->new(Perl::JIT::AST::Hash->new(INT));

# Check roundtripping
for my $type (SCALAR, DOUBLE, INT, UNSIGNED_INT,
              GV, OPAQUE, UNSPECIFIED, $str_type,
              $ary_uint_type, $hash_uint_type,
              $ary_hash_double_type,
              $ary_hash_scalar_type,
              $ary_hash_int_type)
{
  $t = minimal_covering_type([$type]);
  ok($t->equals($type), $type->to_string() . " type roundtrips through minimal_covering_type");

  $t = minimal_covering_type([$type, $type]);
  ok($t->equals($type), "2x " . $type->to_string() . " type roundtrips through minimal_covering_type");
}

ok(minimal_covering_type([SCALAR, DOUBLE, INT, INT, INT, $str_type,
                          UNSIGNED_INT, UNSPECIFIED, OPAQUE])
   ->equals(OPAQUE), "Opaque trumps other scalar types");

ok(minimal_covering_type([SCALAR, DOUBLE, INT, INT, INT, $str_type,
                          UNSIGNED_INT, UNSPECIFIED])
   ->equals(SCALAR), "Scalar trumps other scalar types (!Opaque)");

ok(minimal_covering_type([DOUBLE, INT, $str_type,
                          UNSIGNED_INT,])
   ->equals(SCALAR), "Covering for num & string is SCALAR");

ok(minimal_covering_type([DOUBLE, INT, UNSIGNED_INT,])
   ->equals(DOUBLE), "Covering for num is DOUBLE");

ok(minimal_covering_type([INT, UNSIGNED_INT,])
   ->equals(DOUBLE), "Covering for UINT and INT is DOUBLE");

ok(minimal_covering_type([INT, UNSIGNED_INT,])
   ->equals(DOUBLE), "Covering for UINT and INT is DOUBLE");

ok(minimal_covering_type([$ary_hash_int_type, $ary_hash_double_type])
   ->equals($ary_hash_double_type), "Covering for Array[Hash[Int]] & A[H[Double]] is the latter");

ok(!defined(minimal_covering_type([$ary_hash_int_type, $ary_hash_double_type, DOUBLE])), "Add double into the mix => undef");

ok(minimal_covering_type([$ary_hash_int_type, $ary_hash_scalar_type, $ary_hash_double_type])
   ->equals($ary_hash_scalar_type), "Covering for Array[Hash[Int]] & A[H[Double]] & A[H[Scalar]] is the latter");

for (DOUBLE, INT, UNSIGNED_INT, SCALAR, OPAQUE, $str_type) {
  ok(minimal_covering_type([UNSPECIFIED, $_])
     ->equals($_), "Covering for ".$_->to_string()." and UNSPECIFIED is the former");
}

my @types = ($ary_uint_type, $hash_uint_type, OPAQUE);
foreach my $left (@types) {
  foreach my $right (@types) {
    next if $left->equals($right);
    ok(!defined(minimal_covering_type([$left, $right])),
       "Covering for ".$left->to_string()." and ".$right->to_string()." doesn't exist");
  }
}

done_testing();
