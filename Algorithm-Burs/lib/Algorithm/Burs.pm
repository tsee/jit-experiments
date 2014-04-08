package Algorithm::Burs;

use strict;
use warnings;

use List::Util qw(min);

sub new {
    my ($class) = @_;
    my $self = bless {
        tags        => {},
        functors    => {},
        func_rules  => [],
        chain_rules => [],
        rules       => [],
        reps        => {},
        transition  => {},
        ordinal     => 0,
    }, $class;

    return $self;
}

sub add_tag {
    my ($self, $tag) = @_;

    return $self->{tags}{$tag} ||= ++$self->{ordinal};
}

sub tag {
    my ($self, $tag) = @_;

    return $self->{tags}{$tag} || die "Unknown tag '$tag'";
}

sub add_functor {
    my ($self, $functor, $arity) = @_;
    my $id = $self->{functors}{$functor} ||= ++$self->{ordinal};

    die "Inconsistent functor arity"
        if exists $self->{arity}{$id} && $self->{arity}{$id} != $arity;
    $self->{arity}{$id} = $arity;

    return $id;
}

sub functor {
    my ($self, $functor) = @_;

    return $self->{functors}{$functor} || die "Unknown functor '$functor'";
}

sub functors {
    my ($self) = @_;

    return $self->{functors};
}

sub add_functor_rule {
    my ($self, $label, $functor, $args, $weight) = @_;
    my $id = @{$self->{rules}};

    push @{$self->{rules}}, {
        type    => 'functor',
        label   => $label,
        weight  => $weight // 0,
        functor => $functor,
        args    => $args,
    };
    push @{$self->{func_rules}}, $id;

    return $id;
}

sub add_chain_rule {
    my ($self, $label, $label_to, $weight) = @_;
    my $id = @{$self->{rules}};

    push @{$self->{rules}}, {
        type    => 'chain',
        from    => $label,
        weight  => $weight // 0,
        to      => $label_to,
    };
    push @{$self->{chain_rules}}, $id;

    return $id;
}

sub rule {
    my ($self, $rule_id) = @_;

    return $self->{rules}[$rule_id];
}

sub rules {
    my ($self) = @_;

    return $self->{rules};
}

sub generate_tables {
    my ($self) = @_;

    local $self->{worklist} = [];
    local $self->{states} = [];
    local $self->{reps} = {};
    local $self->{transition} = {};
    local $self->{leaves} = {};
    local $self->{op_map} = {};

    # print "FUNCTORS ", Data::Dumper::Dumper($self->{functors});
    # print "TAGS ", Data::Dumper::Dumper($self->{tags});

    $self->_compute_leaf_states();

    while (@{$self->{worklist}}) {
        my $state_id = shift @{$self->{worklist}};
        for my $functor (values %{$self->{functors}}) {
            $self->_compute_transitions($functor, $state_id);
        }
    }

    # print "LEAVES ", Data::Dumper::Dumper($self->{leaves});
    # print "STATES ", Data::Dumper::Dumper($self->{states});
    # print "OP MAP ", Data::Dumper::Dumper($self->{op_map});
    # print "REPS ", Data::Dumper::Dumper($self->{reps});
    # print "TRANS ", Data::Dumper::Dumper($self->{transitions});
    my @states;

    for my $state (@{$self->{states}}) {
        push @states, { map { $_ => $state->{items}{$_}[2] }
                            sort keys %{$state->{items}} };
    }

    return {
        leaves_map  => $self->{leaves},
        op_map      => $self->{op_map},
        states      => \@states,
        transitions => {
            map { $_ => [values $self->{transitions}{$_}] }
                sort keys %{$self->{transitions}}
        },
    };
}

sub _compute_leaf_states {
    my ($self) = @_;
    my %states;

    # TODO map for leaf rules
    for my $rule_id (@{$self->{func_rules}}) {
        my $rule = $self->{rules}[$rule_id];
        next if $rule->{args};
        my $state = $states{$rule->{functor}} ||= {
            items => {},
        };

        $state->{items}{$rule->{label}} = [$rule_id, $rule->{weight}, [$rule_id]];
    }

    for my $functor (sort keys %states) {
        my $state = $states{$functor};
        $self->_normalize_costs($state);
        $self->_closure($state);
        push @{$self->{states}}, $state;
        push @{$self->{worklist}}, $#{$self->{states}};
        $self->{leaves}{$functor} = $#{$self->{states}};
    }
}

sub _normalize_costs {
    my ($self, $state) = @_;
    return unless scalar keys %{$state->{items}};

    my $delta = min(map $_->[1], values %{$state->{items}});

    for my $item (values %{$state->{items}}) {
        $item->[1] -= $delta;
    }
}

sub _closure {
    my ($self, $state) = @_;
    my $changed;

    # TODO map RHS -> rule for chain rules
    do {
        $changed = 0;
        for my $rule_id (@{$self->{chain_rules}}) {
            my $rule = $self->{rules}[$rule_id];
            my $rule_to = $state->{items}{$rule->{to}};
            next unless $rule_to;
            my $weight = $rule->{weight} + $rule_to->[1];
            my $state_rule = $state->{items}{$rule->{from}};
            my $rules = [@{$rule_to->[2]}, $rule_id];
            if (!$state_rule) {
                $state->{items}{$rule->{from}} = [$rule_id, $weight, $rules];
                $changed = 1;
            } elsif ($weight < $state_rule->[1]) {
                $state->{items}{$rule->{from}} = [$rule_id, $weight, $rules];
                $changed = 1;
            }
        }
    } while ($changed);
}

sub _compute_transitions {
    my ($self, $functor, $state_id) = @_;
    my $state = $self->{states}[$state_id];

    my $reps = $self->{reps}{$functor} ||= [];
    my $arity = $self->{arity}{$functor};
  ARITY: for my $i (0..$arity - 1) {
        my $pstate = $self->_project($functor, $i, $state);
        next unless keys %{$pstate->{items}};
        my $pstate_id = $self->_in_states($pstate, $reps->[$i]);
        if ($pstate_id != -1) {
            $self->{op_map}{$functor}[$i]{$state_id} = $pstate_id;
        } else {
            push @{$reps->[$i]}, $pstate;
            $pstate_id = $#{$reps->[$i]};
            $self->{op_map}{$functor}[$i]{$state_id} = $pstate_id;
            my $index = 0;
            my (@indices) = (0) x ($arity);

            $indices[$i] = $#{$reps->[$i]};

            for my $j (0..$arity - 1) {
                next ARITY if $#{$reps->[$j]} == -1;
            }

          ENUMERATE: for (;;) {
                my $result = {
                    items => {},
                };

              MIN_COST: for my $rule_id (@{$self->{func_rules}}) {
                    my $rule = $self->{rules}[$rule_id];
                    next unless $rule->{functor} == $functor;
                    next MIN_COST unless exists $pstate->{items}{$rule->{args}[$i]};
                    my $cost = $rule->{weight} + $pstate->{items}{$rule->{args}[$i]}[1];

                    for my $j (0..$arity - 1) {
                        next if $j == $i;
                        my $state_j = $reps->[$j][$indices[$j]];
                        next MIN_COST unless exists $state_j->{items}{$rule->{args}[$j]};
                        $cost += $state_j->{items}{$rule->{args}[$j]}[1];
                    }

                    if (!exists $result->{items}{$rule->{label}} || $result->{items}{$rule->{label}}[1] > $cost) {
                        $result->{items}{$rule->{label}} = [$rule_id, $cost, [$rule_id]];
                    }
                }

                # TODO trim

                $self->_normalize_costs($result);
                my $result_id = $self->_in_states($result, $self->{states});
                if ($result_id == -1) {
                    $self->_closure($result);
                    push @{$self->{states}}, $result;
                    push @{$self->{worklist}}, $#{$self->{states}};
                    $result_id = $#{$self->{states}};
                }
                push @{$self->{transitions}{$functor}}, [@indices, $result_id];

              STEP: for my $j (0..$arity) {
                    next if $j == $i;
                    last ENUMERATE if $j == $arity;
                    if ($indices[$j] < $#{$reps->[$j]}) {
                        ++$indices[$j];
                        last STEP;
                    } else {
                        $indices[$j] = 0;
                    }
                }
            }
        }
    }
}

sub _project {
    my ($self, $functor, $i, $state) = @_;
    my $pstate = {
        items => {},
    };

    for my $rule_id (@{$self->{func_rules}}) {
        my $rule = $self->{rules}[$rule_id];
        next unless $rule->{functor} == $functor;
        my $n = $rule->{args}[$i];

        if (exists $state->{items}{$n}) {
            $pstate->{items}{$n} = [-1, $state->{items}{$n}[1], $state->{items}{$n}[2]];
        }
    }

    $self->_normalize_costs($pstate);

    return $pstate;
}

sub _in_states {
    my ($self, $state, $states) = @_;

    for my $i (0..$#$states) {
        return $i if _state_equals($state, $states->[$i]);
    }

    return -1;
}

sub _state_equals {
    my ($s1, $s2) = @_;

    return 0 unless scalar keys %{$s1->{items}} == scalar keys %{$s2->{items}};
    for my $k (keys %{$s1->{items}}) {
        return 0 unless exists $s2->{items}{$k};
        my $v1 = $s1->{items}{$k};
        my $v2 = $s2->{items}{$k};

        return 0 if $v1->[0] != $v2->[0];
        return 0 if $v1->[1] != $v2->[1];
    }

    return 1;
}

1;
