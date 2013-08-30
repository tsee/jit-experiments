#!/usr/bin/env perl

use t::lib::Perl::JIT::Test tests => 11;

use Perl::JIT;

# Makes the assumption that srand() is never going to be JITted

sub test_not_jitted {
    my ($a) = @_;

    my $x = srand($a + 4) + 8;

    return $x;
}

sub test_not_jitted_chain {
    my ($a) = @_;

    # tests the JIT does not break the optree
    my $x = srand(srand($a + 4)) + 8;

    return $x;
}

sub test_typed_scalar {
    typed Opaque ($a) = @_;

    my $x = 4 + $a + 8;

    return $x;
}

sub test_typed_scalar_nomagic {
    typed Opaque ($a) = @_;

    # ord/chr used to stop magic from propagating outwards
    my $x = 4 + ord(chr($a - 1)) + 8;

    return $x;
}

is_jitting(\&test_not_jitted, [{name => 'add'}]);
is(test_not_jitted(42), 54);
is_not_jitted(\&test_not_jitted, [{name => 'srand'}]);

is_jitting(\&test_not_jitted_chain, [{name => 'add'}]);
is(test_not_jitted(42), 54);
is_not_jitted(\&test_not_jitted_chain,
              [{name => 'srand'},
               {name => 'srand', first => {name => 'srand'}}]);

is_not_jitting(\&test_typed_scalar, [{name => 'add'}]);
is(test_typed_scalar(42), 54);

is_jitting(\&test_typed_scalar_nomagic, [{name => 'add'}]);
is(test_typed_scalar_nomagic(43), 54);
is_not_jitted(\&test_typed_scalar_nomagic, [{name => 'subtract'}]);
