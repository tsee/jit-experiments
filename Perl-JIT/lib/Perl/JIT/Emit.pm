package Perl::JIT::Emit;

use v5.14;

use Moo;

use B::Generate;
use B::Replace;
use B;

use Perl::JIT;

use LibJIT::API qw(:all);
use LibJIT::PerlAPI qw(:all);

no warnings $] < 5.018 ? 'redefine' : 'experimental';
use warnings 'redefine';

has jit_context => ( is => 'ro' );

sub BUILD {
    my ($self) = @_;

    $self->{jit_context} = jit_context_create();
}

sub jit_sub {
    my ($self, $sub) = @_;
    my $cv = B::svref_2object($sub);
    my @asts = Perl::JIT::find_jit_candidates($sub);

    for my $ast (@asts) {
        my $op = $self->jit_tree($ast);

        # TODO add B::Generate API taking the CV
        B::Replace::replace_tree($cv->ROOT, $ast->get_perl_op, $op);
    }
}

sub jit_tree {
    my ($self, $ast) = @_;
    my $op = B::OP->new('null', 0);
    my $cxt = $self->jit_context;

    jit_context_build_start($self->jit_context);

    my $signature = jit_type_create_signature(
        jit_abi_cdecl, jit_type_void_ptr, [jit_type_void_ptr], 1);
    my $fun = jit_function_create($cxt, $signature);
    # TODO cleanup $thx passing in LibJIT::PerlAPI
    my $thx = jit_value_get_param($fun, 0);

    my $val = $self->_jit_emit($fun, $thx, $ast);

    jit_function_compile($fun);
    jit_context_build_end($self->jit_context);

    # TODO this need to switch based on op type, flags, ...
    my $targ = pa_get_targ($fun, $thx);
    pa_sv_set_nv($fun, $thx, $targ, $val);
    pa_push_sv($fun, $thx, $targ);
    jit_insn_return($fun, pa_get_op_next($fun, $thx));

    $op->ppaddr(jit_function_to_closure($fun));
    $op->targ($ast->get_perl_op->targ);
    $op->next($ast->get_perl_op->next);

    return $op;
}

sub _jit_emit {
    my ($self, $fun, $thx, $ast) = @_;

    # TODO wrap integer constants and remove hardcoded values
    # TODO only doubles for now...
    given ($ast->get_type) {
        when (0) {
            return $self->_jit_emit_const($fun, $thx, $ast);
        }
        when (1) {
            my $padix = jit_value_create_nint_constant($fun, jit_type_nint, $ast->get_perl_op->targ);
            my $sv = pa_get_pad_sv($fun, $thx, $padix);

            return pa_sv_2nv($fun, $thx, $sv);
        }
        when (2) {
            # nothing to do here
        }
        when (3) {
            return $self->_jit_emit_op($fun, $thx, $ast);
        }
    }
}

sub _jit_emit_op {
    my ($self, $fun, $thx, $ast) = @_;

    given ($ast->get_optype) {
        when (10) {
            my $v1 = $self->_jit_emit($fun, $thx, $ast->get_left_kid);
            my $v2 = $self->_jit_emit($fun, $thx, $ast->get_right_kid);

            return jit_insn_add($fun, $v1, $v2);
        }
        default {
            die "I'm sorry, I've not been implemented yet";
        }
    }
}

sub _jit_emit_const {
    my ($self, $fun, $thx, $ast) = @_;

    # TODO fix the type to match IV/UV/NV
    given ($ast->get_const_type) {
        when (0) {
            return jit_value_create_float64_constant($fun, jit_type_float64, $ast->get_dbl_value);
        }
        when (1) {
            return jit_value_create_long_constant($fun, jit_type_long, $ast->get_int_value);
        }
        when (2) {
            return jit_value_create_long_constant($fun, jit_type_ulong, $ast->get_uint_value);
        }
    }
}

1;
