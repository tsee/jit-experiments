package Perl::JIT::Emit;

use v5.14;

use Moo;

use B::Generate;
use B::Replace;
use B qw(OPf_STACKED);

use Perl::JIT qw(:all);

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

    my $fun = pa_create_pp($cxt);
    $self->_jit_emit_root($fun, $ast);

    jit_function_compile($fun);
    jit_context_build_end($self->jit_context);

    $op->ppaddr(jit_function_to_closure($fun));
    $op->targ($ast->get_perl_op->targ);
    $op->next($ast->get_perl_op->next);

    return $op;
}

sub _jit_emit_root {
    my ($self, $fun, $ast) = @_;
    my $val = $self->_jit_emit($fun, $ast);

    $self->_jit_emit_return($fun, $ast, $val) if $val;

    jit_insn_return($fun, pa_get_op_next($fun));
}

sub _jit_emit_return {
    my ($self, $fun, $ast, $val) = @_;

    given ($ast->op_class) {
        when (pj_opc_binop) {
            # the assumption here is that the OPf_STACKED assignment
            # has been handled by _jit_emit below, and here we only need
            # to handle casses like '$x = $y += 7'
            my $targ = pa_get_targ($fun);

            pa_sv_set_nv($fun, $targ, $val);
            pa_push_sv($fun, $targ);
        }
        when (pj_opc_unop) {
            my $targ = pa_get_targ($fun);

            pa_sv_set_nv($fun, $targ, $val);
            pa_push_sv($fun, $targ);
        }
        default {
            pa_push_sv($fun, pa_sv_2mortal(pa_new_sv_nv($val)));
        }
    }
}

sub _jit_emit {
    my ($self, $fun, $ast) = @_;

    # TODO only doubles for now...
    given ($ast->get_type) {
        when (pj_ttype_constant) {
            return $self->_jit_emit_const($fun, $ast);
        }
        when (pj_ttype_variable) {
            my $padix = jit_value_create_nint_constant($fun, jit_type_nint, $ast->get_perl_op->targ);

            return pa_get_pad_sv($fun, $padix);
        }
        when (pj_ttype_optree) {
            # nothing to do here
        }
        when (pj_ttype_op) {
            return $self->_jit_emit_op($fun, $ast);
        }
    }
}

sub _to_nv {
    my ($self, $fun, $val) = @_;
    my $type = jit_value_get_type($val);

    if ($$type == ${jit_type_float64()}) {
        return $val;
    } elsif ($$type == ${jit_type_void_ptr()}) {
        return pa_sv_nv($fun, $val);
    } else {
        die "Handle more coercion cases";
    }
}

sub _jit_emit_op {
    my ($self, $fun, $ast) = @_;

    given ($ast->get_optype) {
        when (pj_binop_add) {
            my $v1 = $self->_jit_emit($fun, $ast->get_left_kid);
            my $v2 = $self->_jit_emit($fun, $ast->get_right_kid);
            my $res = jit_insn_add($fun, $self->_to_nv($fun, $v1), $self->_to_nv($fun, $v2));

            # this should be a pj_binop_add_assing; or maybe not
            if ($ast->get_perl_op->flags & OPf_STACKED) {
                pa_sv_set_nv($fun, $v1, $res);
            }

            return $res;
        }
        default {
            die "I'm sorry, I've not been implemented yet";
        }
    }
}

sub _jit_emit_const {
    my ($self, $fun, $ast) = @_;

    # TODO fix the type to match IV/UV/NV
    given ($ast->get_const_type) {
        when (pj_double_type) {
            return jit_value_create_float64_constant($fun, jit_type_float64, $ast->get_dbl_value);
        }
        when (pj_int_type) {
            return jit_value_create_long_constant($fun, jit_type_long, $ast->get_int_value);
        }
        when (pj_uint_type) {
            return jit_value_create_long_constant($fun, jit_type_ulong, $ast->get_uint_value);
        }
    }
}

1;
