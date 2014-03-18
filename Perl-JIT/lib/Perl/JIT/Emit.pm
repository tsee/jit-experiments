package Perl::JIT::Emit;
use strict;
use warnings;
use Exporter qw(import);

use Perl::JIT qw(:all);
use B::Replace;

our @EXPORT_OK = qw(jit_sub concise_dump);
our %EXPORT_TAGS = ( all => \@EXPORT_OK );

sub jit_sub {
    my ($sub) = @_;

    my $ops = _jit_sub($sub);

    # TODO add C API for B::Replace and remove this horror
    for my $op (@$ops) {
        if (shift @$op eq 'replace') {
            B::Replace::replace_sequence(@$op);
        } else {
            B::Replace::detach_tree(@$op);
        }
    }
}

sub concise_dump {
    require B::Concise;
    B::Concise::reset_sequence();
    B::Concise::compile('', '', $_[0])->();
}

1;
