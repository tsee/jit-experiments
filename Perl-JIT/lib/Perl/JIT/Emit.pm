package Perl::JIT::Emit;
use strict;
use warnings;

use Perl::JIT qw(:all);

use B::Replace;

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

# sub _concise_dump {
#     my ($self) = @_;
#     require B::Concise;
#     B::Concise::reset_sequence();
#     B::Concise::compile('', '', $self->current_cv)->();
# }

1;
