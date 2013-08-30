package Perl::JIT::Emit;

use v5.14;

use Moo;

use warnings;
# This is to make given/when work on 5.14 and 5.18. *sigh*, and it
# needs to be after 'use Moo'
no warnings $] < 5.018 ? 'redefine' : 'experimental';
use warnings 'redefine';

use B::Generate;
use B::Replace;
use B qw(OPf_KIDS);

use Perl::JIT qw(:all);
use Perl::JIT::Types qw(:all);

use LibJIT::API qw(:all);
use LibJIT::PerlAPI qw(:all);

has jit_context => ( is => 'ro' );
has current_cv  => ( is => 'ro' );
has subtrees    => ( is => 'ro' );
has _fun        => ( is => 'ro' );

sub BUILD {
    my ($self) = @_;

    $self->{jit_context} ||= jit_context_create();
}

sub clone {
    my ($self) = @_;

    return Perl::JIT::Emit->new({
        jit_context => $self->jit_context,
        current_cv  => $self->current_cv,
    });
}

sub jit_sub {
    my ($self, $sub) = @_;
    my @asts = Perl::JIT::find_jit_candidates($sub);

    jit_context_build_start($self->jit_context);

    local $self->{current_cv} = B::svref_2object($sub);

    $self->process_jit_candidates([@asts]);

    jit_context_build_end($self->jit_context);
}

sub process_jit_candidates {
    my ($self, $asts) = @_;

    while (my $ast = shift @$asts) {
        next if $ast->get_type == pj_ttype_variable ||
                $ast->get_type == pj_ttype_constant;

        if ($ast->get_type == pj_ttype_nulloptree) {
            # The tree has been marked for deletion, so just detach it
            B::Replace::detach_tree($self->current_cv->ROOT, $ast->get_perl_op);
        }
        elsif ($self->is_jittable($ast)) {
            local $self->{subtrees} = [];
            my $op = $self->jit_tree($ast);

            # TODO add B::Replace API taking the CV
            B::Replace::replace_tree($self->current_cv->ROOT, $ast->get_perl_op, $op);
        }
        else {
            unshift @$asts, $ast->get_kids;
        }
    }
}

sub jit_tree {
    my ($self, $ast) = @_;

    local $self->{_fun} = my $fun = pa_create_pp($self->jit_context);
    $self->_jit_emit_root($ast);

    # here it'd be nice to use custom ops, but they are registered by
    # PP function address; we could use a trampoline address (with
    # just an extra jump, but then we'd need to store the pointer to the
    # JITted function as an extra op member
    my $op;

    if (@{$self->subtrees}) {
        my $tree = $self->subtrees;

        $tree->[$_]->sibling($tree->[$_ + 1]) for 0 .. $#$tree;
        $tree->[-1]->sibling(0);
        $op = B::LISTOP->new('list', OPf_KIDS, $tree->[0], @$tree > 1 ? $tree->[-1] : 0);
    } else {
        $op = B::OP->new('stub', 0);
    }

    #jit_dump_function(\*STDOUT, $fun, "foo");
    jit_function_compile($fun);

    $op->ppaddr(jit_function_to_closure($fun));
    $op->targ($ast->get_perl_op->targ);

    return $op;
}

my %Jittable_Ops = map { $_ => 1 } (
    pj_binop_add, pj_binop_subtract, pj_binop_multiply, pj_binop_divide,
    pj_binop_bool_and,

    pj_unop_negate, pj_unop_abs, pj_unop_sin, pj_unop_cos, pj_unop_sqrt,
    pj_unop_log, pj_unop_exp, pj_unop_bool_not, pj_unop_perl_int,
);

my %Ops_Propagating_Magic = (
    (map { $_ => 1 } (
        pj_binop_add, pj_binop_subtract, pj_binop_multiply, pj_binop_divide,
        pj_binop_bool_and,

        pj_unop_negate, pj_unop_abs, pj_unop_sin, pj_unop_cos, pj_unop_sqrt,
        pj_unop_log, pj_unop_exp, pj_unop_bool_not, pj_unop_perl_int,
    )),
    (map { $_ => 0 } (
        pj_unop_ord, pj_unop_chr,
    )),
);

sub is_jittable {
    my ($self, $ast) = @_;

    given ($ast->get_type) {
        when (pj_ttype_constant) { return 1 }
        when (pj_ttype_variable) { return 1 }
        when (pj_ttype_optree) { return 0 }
        when (pj_ttype_nulloptree) { return 1 }
        when (pj_ttype_op) {
            my $known = $Jittable_Ops{$ast->get_optype};
            my $propagates = $Ops_Propagating_Magic{$ast->get_optype};

            return 0 unless $known;
            return 1 unless $propagates;
            return !$self->needs_excessive_magic($ast);
        }
        default { return 0 }
    }
}

sub needs_excessive_magic {
    my ($self, $ast) = @_;
    my @nodes = $ast;

    while (@nodes) {
        my $node = shift @nodes;

        return 1 if $node->get_type == pj_ttype_variable &&
                    $node->get_value_type->equals(OPAQUE);
        next unless $node->get_type == pj_ttype_op;

        my $known = $Jittable_Ops{$node->get_optype};
        my $propagates = $Ops_Propagating_Magic{$node->get_optype};

        next if !$known || !$propagates;
        push @nodes, $node->get_kids;
    }

    return 0;
}

sub _jit_emit_root {
    my ($self, $ast) = @_;
    my ($val, $type) = $self->_jit_emit($ast, UNSPECIFIED);

    $self->_jit_emit_return($ast, $val, $type) if $val;

    jit_insn_return($self->_fun, pa_get_op_next($self->_fun));
}

sub _jit_emit_return {
    my ($self, $ast, $val, $type) = @_;
    my $fun = $self->_fun;

    given ($ast->op_class) {
        when (pj_opc_binop) {
            # the assumption here is that the OPf_STACKED assignment
            # has been handled by _jit_emit below, and here we only need
            # to handle cases like '$x = $y += 7'
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
    my ($self, $ast, $type) = @_;

    # TODO only doubles for now...
    given ($ast->get_type) {
        when (pj_ttype_constant) {
            return $self->_jit_emit_const($ast, $type);
        }
        when (pj_ttype_variable) {
            my $fun = $self->_fun;
            my $padix = jit_value_create_nint_constant($fun, jit_type_nint, $ast->get_perl_op->targ);

            return (pa_get_pad_sv($fun, $padix), UNSPECIFIED);
        }
        when (pj_ttype_optree) {
            return $self->_jit_emit_optree($ast, $type);
        }
        when (pj_ttype_nulloptree) {
            # the optree has been marked for oblivion (for example the
            # synthetic call to attributes->import generated by my $a : Int)
            # just kill it
            B::Replace::detach_tree($self->current_cv->ROOT, $ast->get_perl_op);

            # since this call must return no value, it is OK to return
            # undef here
            return (undef, undef);
        }
        when (pj_ttype_op) {
            if ($self->is_jittable($ast)) {
                return $self->_jit_emit_op($ast, $type);
            } else {
                return $self->_jit_emit_optree_jit_kids($ast, $type);
            }
        }
        default {
            return $self->_jit_emit_optree_jit_kids($ast, $type);
        }
    }
}

sub _type_is_integer {
    my ($type) = @_;

    return $type->equals(INT) || $type->equals(UNSIGNED_INT);
}

sub _type_is_numeric {
    my ($type) = @_;

    return $type->equals(INT) || $type->equals(UNSIGNED_INT) || $type->equals(DOUBLE);
}

sub _to_nv {
    my ($self, $val, $type) = @_;

    if ($type->equals(DOUBLE)) {
        return $val;
    } elsif (_type_is_integer($type)) {
        return jit_insn_convert($self->_fun, $val, jit_type_NV, 0);
    } elsif ($type->equals(UNSPECIFIED)) {
        return pa_sv_nv($self->_fun, $val);
    } else {
        die "Handle more coercion cases";
    }
}

sub _to_numeric_type {
    my ($self, $val, $type) = @_;

    if (_type_is_numeric($type)) {
        return $val;
    } elsif ($type->equals(UNSPECIFIED)) {
        return pa_sv_nv($self->_fun, $val); # somewhat dubious
    } else {
        die "Handle more coercion cases";
    }
}

sub _to_type {
    my ($self, $val, $type, $to_type) = @_;

    if ($to_type->equals($type)) {
        return $val;
    } elsif ($to_type->equals(DOUBLE)) {
        return $self->_to_nv($val, $type);
    } else {
        die "Handle more coercion cases";
    }
}

sub _jit_emit_optree_jit_kids {
    my ($self, $ast, $type) = @_;

    $self->clone->process_jit_candidates([$ast->get_kids]);

    return $self->_jit_emit_optree($ast, $type);
}

sub _jit_emit_optree {
    my ($self, $ast) = @_;

    # unfortunately there is (currently) no way to clone an optree,
    # so just detach the ops from the root tree
    B::Replace::detach_tree($self->current_cv->ROOT, $ast->get_perl_op, 1);
    $ast->get_perl_op->next(0);
    push @{$self->subtrees}, $ast->get_perl_op;

    my $op = jit_value_create_long_constant($self->_fun, jit_type_ulong, ${$ast->start_op});
    pa_call_runloop($self->_fun, $op);

    # TODO only works for scalar context
    return (pa_pop_sv($self->_fun), UNSPECIFIED);
}

sub _jit_emit_op {
    my ($self, $ast, $type) = @_;
    my $fun = $self->_fun;

    given ($ast->op_class) {
        when (pj_opc_binop) {
            my ($res, $restype);
            my ($v1, $v2, $t1, $t2);
            if (not $ast->evaluates_kids_conditionally) {
                ($v1, $t1) = $self->_jit_emit($ast->get_left_kid, DOUBLE);
                ($v2, $t2) = $self->_jit_emit($ast->get_right_kid, DOUBLE);
                $restype = DOUBLE;
            }

            given ($ast->get_optype) {
                when (pj_binop_add) {
                    $res = jit_insn_add($fun, $self->_to_nv($v1, $t1), $self->_to_nv($v2, $t2));
                }
                when (pj_binop_subtract) {
                    $res = jit_insn_sub($fun, $self->_to_nv($v1, $t1), $self->_to_nv($v2, $t2));
                }
                when (pj_binop_multiply) {
                    $res = jit_insn_mul($fun, $self->_to_nv($v1, $t1), $self->_to_nv($v2, $t2));
                }
                when (pj_binop_divide) {
                    $res = jit_insn_div($fun, $self->_to_nv($v1, $t1), $self->_to_nv($v2, $t2));
                }
                when (pj_binop_bool_and) {
                    # This is all a bit awkward because we need to check $a
                    # of $a && $b first, then return $a (but not the converted
                    # value of $a!) then return unconverted $b. On top of that,
                    # we need to only evaluate $b (which could be a big subtree)
                    # only if $a was false - Perl's short-circuiting doesn't happen
                    # automatically at this level.
                    # FIXME should this do a better emulation of the Perl truth check?
                    #       Something along the lines of "if it's an SV, compare ptr to
                    #       &PL_sv_undef. If same, then false & return. If not, then try
                    #       the numeric conversion."?
                    # FIXME Right now assumes type jit_type_void_ptr (==SV*).
                    #       That's obviously totally broken.

                    # note that we ask subtrees to return a value with desired
                    # type, but we need coercion when that is not the case!

                    my $endlabel = jit_label_undefined;
                    $res = $self->_jit_create_value($type);
                    $restype = $type;

                    # If value is false, then go with v1 and never look at v2
                    ($v1, $t1) = $self->_jit_emit($ast->get_left_kid, $type);
                    jit_insn_store($fun, $res, $self->_to_type($v1, $t1, $type));
                    my $tmp = $self->_to_numeric_type($v1, $t1, $type);
                    jit_insn_branch_if_not($fun, $tmp, $endlabel);

                    # Left is true, move to right operand
                    ($v2, $t2) = $self->_jit_emit($ast->get_right_kid, $type);
                    jit_insn_store($fun, $res, $self->_to_type($v2, $t2, $type));

                    # endlabel; done.
                    jit_insn_label($fun, $endlabel);
                }
                default {
                    return $self->_jit_emit_optree_jit_kids($ast, $type);
                }
            }

            if ($ast->is_assignment_form) {
                pa_sv_set_nv($fun, $v1, $res);
            }

            return ($res, $restype);
        }
        when (pj_opc_unop) {
            my ($v1, $t1) = $self->_jit_emit($ast->get_kid, DOUBLE);
            my ($res, $restype);

            $restype = DOUBLE;

            given ($ast->get_optype) {
                when (pj_unop_negate) {
                    $res = jit_insn_neg($fun, $self->_to_nv($v1, $t1));
                }
                when (pj_unop_abs) {
                    $res = jit_insn_abs($fun, $self->_to_nv($v1, $t1));
                }
                when (pj_unop_sin) {
                    $res = jit_insn_sin($fun, $self->_to_nv($v1, $t1));
                }
                when (pj_unop_cos) {
                    $res = jit_insn_cos($fun, $self->_to_nv($v1, $t1));
                }
                when (pj_unop_sqrt) {
                    $res = jit_insn_sqrt($fun, $self->_to_nv($v1, $t1));
                }
                when (pj_unop_log) {
                    $res = jit_insn_log($fun, $self->_to_nv($v1, $t1));
                }
                when (pj_unop_exp) {
                    $res = jit_insn_exp($fun, $self->_to_nv($v1, $t1));
                }
                when (pj_unop_bool_not) {
                    $res = jit_insn_to_not_bool($fun, $self->_to_numeric_type($v1, $t1));
                    $restype = INT;
                }
                when (pj_unop_perl_int) {
                    if (_type_is_integer($t1)) {
                        $res = $v1;
                    }
                    else {
                        my $val = $self->_to_numeric_type($v1, $t1);
                        my $endlabel = jit_label_undefined;
                        my $neglabel = jit_label_undefined;

                        # if value < 0.0, then goto neglabel
                        my $constval = jit_value_create_NV_constant($fun, 0.0);
                        my $tmpval = jit_insn_lt($fun, $val, $constval);
                        jit_insn_branch_if($fun, $tmpval, $neglabel);

                        # else use floor, then goto endlabel
                        $res = jit_insn_floor($fun, $val);
                        jit_insn_branch($fun, $endlabel);

                        # neglabel: use ceil, fall through to endlabel
                        jit_insn_label($fun, $neglabel);
                        my $tmprv = jit_insn_ceil($fun, $val);
                        jit_insn_store($fun, $res, $tmprv);

                        # endlabel; done.
                        jit_insn_label($fun, $endlabel);
                    }
                    $restype = INT;
                }
                default {
                    return $self->_jit_emit_optree_jit_kids($ast, $type);
                }
            }

            return ($res, $restype);
        }
        default {
            return $self->_jit_emit_optree_jit_kids($ast, $type);
        }
    }
}

sub _jit_create_value {
    my ($self, $type) = @_;
    my $fun = $self->_fun;

    given ($type->tag) {
        when (pj_int_type) {
            return jit_value_create($fun, jit_type_IV);
        }
        when (pj_uint_type) {
            return jit_value_create($fun, jit_type_UV);
        }
        when (pj_double_type) {
            return jit_value_create($fun, jit_type_NV);
        }
        default {
            return jit_value_create($fun, jit_type_void_ptr);
        }
    }
}

sub _jit_emit_const {
    my ($self, $ast, $type) = @_;
    my $fun = $self->_fun;

    given ($ast->const_type) {
        when (pj_double_type) {
            return (jit_value_create_NV_constant($fun, $ast->get_dbl_value),
                    DOUBLE)
        }
        when (pj_int_type) {
            return (jit_value_create_IV_constant($fun, $ast->get_int_value),
                    INT);
        }
        when (pj_uint_type) {
            return (jit_value_create_UV_constant($fun, $ast->get_uint_value),
                    UNSIGNED_INT);
        }
        default {
            die("Cannot generate code for string constants yet");
        }
    }
}

1;
