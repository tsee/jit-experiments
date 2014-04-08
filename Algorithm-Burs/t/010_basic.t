#!/usr/bin/env perl

use strict;
use warnings;

use Test::More;
use Test::Differences;
use Algorithm::Burs::Perl;

sub node {
    my ($label, @args) = @_;

    return {label => $label, @args ? (args => \@args) : ()};
}

my $burs = Algorithm::Burs::Perl->new;

$burs->add_functor_rule('Expr' => 'add', ['Expr', 'Expr'], 2, sub {
    my ($node, @args) = @_;

    return {label => 'Add', args => \@args};
});
$burs->add_functor_rule('Expr' => 'add', ['Expr', 'Const'], 1, sub {
    my ($node, @args) = @_;

    return {label => 'Addc', args => \@args};
});
$burs->add_chain_rule('Expr' => 'Const', 0);
$burs->add_chain_rule('Expr' => 'Var', 0);
$burs->add_functor_rule('Const' => 'const', undef, 0);
$burs->add_functor_rule('Var' => 'var', undef, 0);

eq_or_diff(
    $burs->run(
        node('add',
             node('add', node('var'), node('const')),
             node('add', node('const'), node('var'))),
    ),
    node('Add',
         node('Addc', node('var'), node('const')),
         node('Add', node('const'), node('var'))),
);

done_testing();
