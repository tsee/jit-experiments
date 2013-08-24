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
use B qw(OPf_STACKED OPf_KIDS);

use Perl::JIT qw(:all);
use Perl::JIT::Types qw(:all);

use LibJIT::API qw(:all);
use LibJIT::PerlAPI qw(:all);

has jit_context => ( is => 'ro' );
has current_cv  => ( is => 'ro' );
has subtrees    => ( is => 'ro' );

sub BUILD {
    my ($self) = @_;

    $self->{jit_context} = jit_context_create();
}

sub jit_sub {
    my ($self, $sub) = @_;
    my @asts = Perl::JIT::find_jit_candidates($sub);
    local $self->{current_cv} = B::svref_2object($sub);
    local $self->{subtrees} = [];

    for my $ast (@asts) {
        if ($ast->get_type == pj_ttype_nulloptree) {
            # The tree has been marked for deletion, so just detach it
            B::Replace::detach_tree($self->current_cv->ROOT, $ast->get_perl_op);
        }
        else {
            my $op = $self->jit_tree($ast);

            # TODO add B::Generate API taking the CV
            B::Replace::replace_tree($self->current_cv->ROOT, $ast->get_perl_op, $op);
        }
    }
}

sub jit_tree {
    my ($self, $ast) = @_;
    my $cxt = $self->jit_context;
    my $op;

    if (@{$self->subtrees}) {
        my $tree = $self->subtrees;

        $tree->[$_]->sibling($tree->[$_ + 1]) for 0 .. $#$tree;
        $op = B::LISTOP->new('stub', OPf_KIDS, $tree->[0], $tree->[-1]);
    } else {
        $op = B::OP->new('stub', 0);
    }

    jit_context_build_start($self->jit_context);

    my $fun = pa_create_pp($cxt);
    $self->_jit_emit_root($fun, $ast);

    #jit_dump_function(\*STDOUT, $fun, "foo");
    jit_function_compile($fun);
    jit_context_build_end($self->jit_context);

    $op->ppaddr(jit_function_to_closure($fun));
    $op->targ($ast->get_perl_op->targ);

    return $op;
}

sub _jit_emit_root {
    my ($self, $fun, $ast) = @_;
    my ($val, $type) = $self->_jit_emit($fun, $ast, UNSPECIFIED);

    $self->_jit_emit_return($fun, $ast, $val, $type) if $val;

    jit_insn_return($fun, pa_get_op_next($fun));
}

sub _jit_emit_return {
    my ($self, $fun, $ast, $val, $type) = @_;

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
    my ($self, $fun, $ast, $type) = @_;

    # TODO only doubles for now...
    given ($ast->get_type) {
        when (pj_ttype_constant) {
            return $self->_jit_emit_const($fun, $ast, $type);
        }
        when (pj_ttype_variable) {
            my $padix = jit_value_create_nint_constant($fun, jit_type_nint, $ast->get_perl_op->targ);

            return (pa_get_pad_sv($fun, $padix), UNSPECIFIED);
        }
        when (pj_ttype_optree) {
            return $self->_jit_emit_optree($fun, $ast, $type);
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
            return $self->_jit_emit_op($fun, $ast, $type);
        }
        default {
            die "I'm sorry, I've not been implemented yet";
        }
    }
}

SCOPE: {
    # FIXME There has to be a better, faster, and more elegant way
    #       than this, but I can't see that right now.
    my %integer_types = map {${$_} => undef} (
        jit_type_sbyte, jit_type_ubyte,
        jit_type_short, jit_type_ushort,
        jit_type_int, jit_type_uint,
        jit_type_nint, jit_type_nuint,
        jit_type_long, jit_type_ulong,
        jit_type_sys_char, jit_type_sys_schar, jit_type_sys_uchar,
        jit_type_sys_short, jit_type_sys_ushort,
        jit_type_sys_int, jit_type_sys_uint,
        jit_type_sys_long, jit_type_sys_ulong,
        jit_type_sys_longlong, jit_type_sys_ulonglong,
        jit_type_IV, jit_type_UV,
    );
    sub _value_is_integer {
        my ($val) = @_;
        my $type = jit_value_get_type($val);
        return exists($integer_types{${$type}});
    }
}

sub _value_is_IV {
    my ($val) = @_;
    my $type = jit_value_get_type($val);
    return $$type == ${jit_type_IV()};
}

sub _value_is_NV {
    my ($val) = @_;
    my $type = jit_value_get_type($val);
    return $$type == ${jit_type_NV()};
}

sub _type_is_integer {
    my ($type) = @_;
    my $tag = $type->tag;
    return $tag == pj_int_type || $tag == pj_uint_type;
}

sub _type_is_numeric {
    my ($type) = @_;
    my $tag = $type->tag;
    return $tag == pj_int_type || $tag == pj_uint_type || $tag == pj_double_type;
}

sub _to_nv {
    my ($self, $fun, $val, $type) = @_;
    my $tag = $type->tag;

    if ($tag == pj_double_type) {
        return $val;
    } elsif ($tag == pj_int_type || $tag == pj_uint_type) {
        return jit_insn_convert($fun, $val, jit_type_NV, 0);
    } elsif ($tag == pj_unspecified_type) {
        return pa_sv_nv($fun, $val);
    } else {
        die "Handle more coercion cases";
    }
}

sub _to_numeric_type {
    my ($self, $fun, $val, $type) = @_;
    my $tag = $type->tag;

    if ($tag == pj_double_type || $tag == pj_int_type || $tag == pj_uint_type) {
        return $val;
    } elsif ($tag == pj_unspecified_type) {
        return pa_sv_nv($fun, $val); # somewhat dubious
    } else {
        die "Handle more coercion cases";
    }
}

sub _to_type {
    my ($self, $fun, $val, $type, $to_type) = @_;
    my $to_tag = $to_type->tag;

    if ($to_tag == $type->tag) {
        return $val;
    } elsif ($to_tag == pj_double_type) {
        return $self->_to_nv($fun, $val, $type);
    } else {
        die "Handle more coercion cases";
    }
}

sub _jit_emit_optree {
    my ($self, $fun, $ast) = @_;

    # unfortunately there is (currently) no way to clone an optree,
    # so just detach the ops from the root tree
    B::Replace::detach_tree($self->current_cv->ROOT, $ast->get_perl_op, 1);
    $ast->get_perl_op->next(0);
    push @{$self->subtrees}, $ast->get_perl_op;

    my $op = jit_value_create_long_constant($fun, jit_type_ulong, ${$ast->get_start_op});
    pa_call_runloop($fun, $op);

    # TODO only works for scalar context
    return (pa_pop_sv($fun), UNSPECIFIED);
}

sub _jit_emit_op {
    my ($self, $fun, $ast, $type) = @_;

    given ($ast->op_class) {
        when (pj_opc_binop) {
            my ($res, $restype);
            my ($v1, $v2, $t1, $t2);
            if (not $ast->evaluates_kids_conditionally) {
                ($v1, $t1) = $self->_jit_emit($fun, $ast->get_left_kid, DOUBLE);
                ($v2, $t2) = $self->_jit_emit($fun, $ast->get_right_kid, DOUBLE);
                $restype = DOUBLE;
            }

            given ($ast->get_optype) {
                when (pj_binop_add) {
                    $res = jit_insn_add($fun, $self->_to_nv($fun, $v1, $t1), $self->_to_nv($fun, $v2, $t2));
                }
                when (pj_binop_subtract) {
                    $res = jit_insn_sub($fun, $self->_to_nv($fun, $v1, $t1), $self->_to_nv($fun, $v2, $t2));
                }
                when (pj_binop_multiply) {
                    $res = jit_insn_mul($fun, $self->_to_nv($fun, $v1, $t1), $self->_to_nv($fun, $v2, $t2));
                }
                when (pj_binop_divide) {
                    $res = jit_insn_div($fun, $self->_to_nv($fun, $v1, $t1), $self->_to_nv($fun, $v2, $t2));
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
                    $res = $self->_jit_create_value($fun, $type);
                    $restype = $type;

                    # If value is false, then go with v1 and never look at v2
                    ($v1, $t1) = $self->_jit_emit($fun, $ast->get_left_kid, $type);
                    jit_insn_store($fun, $res, $self->_to_type($fun, $v1, $t1, $type));
                    my $tmp = $self->_to_numeric_type($fun, $v1, $t1, $type);
                    jit_insn_branch_if_not($fun, $tmp, $endlabel);

                    # Left is true, move to right operand
                    ($v2, $t2) = $self->_jit_emit($fun, $ast->get_right_kid, $type);
                    jit_insn_store($fun, $res, $self->_to_type($fun, $v2, $t2, $type));

                    # endlabel; done.
                    jit_insn_label($fun, $endlabel);
                }
                default {
                    die "I'm sorry, I've not been implemented yet";
                }
            }

            # this should be a pj_binop_<xxx>_assign; or maybe better a flag
            if ($ast->get_perl_op->flags & OPf_STACKED) {
                pa_sv_set_nv($fun, $v1, $res);
            }

            return ($res, $restype);
        }
        when (pj_opc_unop) {
            my ($v1, $t1) = $self->_jit_emit($fun, $ast->get_kid, DOUBLE);
            my ($res, $restype);

            $restype = DOUBLE;

            given ($ast->get_optype) {
                when (pj_unop_negate) {
                    $res = jit_insn_neg($fun, $self->_to_nv($fun, $v1, $t1));
                }
                when (pj_unop_abs) {
                    $res = jit_insn_abs($fun, $self->_to_nv($fun, $v1, $t1));
                }
                when (pj_unop_sin) {
                    $res = jit_insn_sin($fun, $self->_to_nv($fun, $v1, $t1));
                }
                when (pj_unop_cos) {
                    $res = jit_insn_cos($fun, $self->_to_nv($fun, $v1, $t1));
                }
                when (pj_unop_sqrt) {
                    $res = jit_insn_sqrt($fun, $self->_to_nv($fun, $v1, $t1));
                }
                when (pj_unop_log) {
                    $res = jit_insn_log($fun, $self->_to_nv($fun, $v1, $t1));
                }
                when (pj_unop_exp) {
                    $res = jit_insn_exp($fun, $self->_to_nv($fun, $v1, $t1));
                }
                when (pj_unop_bool_not) {
                    $res = jit_insn_to_not_bool($fun, $self->_to_numeric_type($fun, $v1, $t1));
                    $restype = INT;
                }
                when (pj_unop_perl_int) {
                    if (_type_is_integer($t1)) {
                        $res = $v1;
                    }
                    else {
                        my $val = $self->_to_numeric_type($fun, $v1, $t1);
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
                    die "I'm sorry, I've not been implemented yet";
                }
            }

            return ($res, $restype);
        }
        default {
            die "I'm sorry, I've not been implemented yet";
        }
    }
}

sub _jit_create_value {
    my ($self, $fun, $type) = @_;

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
    my ($self, $fun, $ast, $type) = @_;

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
    }
}

1;
