package Algorithm::Burs::Perl;

use strict;
use warnings;

use Algorithm::Burs;
use Data::Dumper;

sub new {
    my ($class) = @_;
    my $self = bless {
        burs      => Algorithm::Burs->new,
        callbacks => {},
        table     => undef,
    }, $class;

    return $self;
}

sub add_functor_rule {
    my ($self, $label, $functor, $args, $weight, $callback) = @_;
    my $burs = $self->{burs};
    my $label_id = $burs->add_tag($label);
    my $functor_id = $burs->add_functor($functor, $args ? scalar @{$args} : 0);
    my $arg_ids;

    if ($args) {
        $arg_ids = [map $burs->add_tag($_), @$args];
    }

    my $rule_id = $burs->add_functor_rule($label_id, $functor_id, $arg_ids, $weight);
    $self->{callbacks}{$rule_id} = $callback if $callback;
}

sub add_chain_rule {
    my ($self, $from, $to, $weight, $callback) = @_;
    my $burs = $self->{burs};
    my $from_id = $burs->add_tag($from);
    my $to_id = $burs->add_tag($to);

    my $rule_id = $burs->add_chain_rule($from_id, $to_id, $weight);
    $self->{callbacks}{$rule_id} = $callback if $callback;
}

sub run {
    my ($self, $node) = @_;

    unless ($self->{tables}) {
        $self->{tables} = $self->{burs}->generate_tables;
    }

    local $self->{labels} = {};

    $self->_label($node);
    $self->_reduce($node, $self->{burs}->tag('Expr')); # XXX
}

sub _label {
    my ($self, $node) = @_;
    my $burs = $self->{burs};
    my $tables = $self->{tables};
    my $args = $node->{args};
    my $functor_id = $burs->functor($node->{label});

    if ($args) {
        my $trans = $tables->{transitions}{$functor_id};
        my @labels;

        for my $i (0..$#{$node->{args}}) {
            my $arg = $node->{args}[$i];
            my $label = $self->_label($arg);
            push @labels, $tables->{op_map}{$functor_id}[$i]{$label};
        }

      LOOKUP: for my $t (@$trans) {
            for my $i (0..$#labels) {
                next LOOKUP if $labels[$i] != $t->[$i];
            }
            return $self->{labels}{$node} = $t->[-1];
        }

        die "Missing transition in table for '@labels'"
    } else {
        return $self->{labels}{$node} = $tables->{leaves_map}{$functor_id};
    }
}

sub _reduce {
    my ($self, $node, $tag_id) = @_;
    my $states = $self->{tables}{states};
    my $label = $self->{labels}{$node};
    my $rule_ids = $states->[$label]{$tag_id};
    my $rule = $self->{burs}->rule($rule_ids->[0]);
    my @args;

    if (my $args = $node->{args}) {
        for my $i (0..$#{$node->{args}}) {
            push @args, $self->_reduce($node->{args}[$i], $rule->{args}[$i]);
        }
    }

    for my $rule_id (@$rule_ids) {
        my $cb = $self->{callbacks}{$rule_id} || sub {
            my ($node, @args) = @_;

            return {label => $node->{label}, @args ? (args => \@args) : ()};
        };
        return $cb->($node, @args);
    }
}

1;
