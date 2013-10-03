#!/usr/bin/env perl
use 5.14.0;
use t::lib::Perl::JIT::ASTTest;

our $global;
our @global;

no warnings 'syntax';
ast_contains(sub { my @a; @a[2] },
             ast_binop(pj_binop_array_slice, ast_constant(2), ast_lexical('@a')));
ast_contains(sub { my $x; @global[$x] },
             ast_binop(pj_binop_array_slice, ast_lexical('$x'), ast_global('@global')));
ast_contains(sub { my $x; @global[$global, 2, $x, @global] },
             ast_binop(
               pj_binop_array_slice,
               ast_list(
                 ast_global('$global'),
                 ast_constant(2),
                 ast_lexical('$x'),
                 ast_global('@global'),
               ),
               ast_global('@global')
             ));

done_testing();
