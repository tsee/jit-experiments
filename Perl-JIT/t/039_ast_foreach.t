#!/usr/bin/env perl

use t::lib::Perl::JIT::ASTTest;

ast_contains(sub { for my $i (0..9) { my $j = $i } },
             ast_foreach(
               ast_lexical('$i'),
               ast_binop(pj_binop_range, ast_constant(0), ast_constant(9)),
               ast_statementsequence([
                   ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_lexical('$i')),
               ]),
             ));
ast_contains(sub { my $i; for $i (0..9) { my $j = $i } },
             ast_foreach(
               ast_lexical('$i'),
               ast_binop(pj_binop_range, ast_constant(0), ast_constant(9)),
               ast_statementsequence([
                   ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_lexical('$i')),
               ]),
             ));
ast_contains(sub { our $i; for $i (0..9) { my $j = $i } },
             ast_foreach(
               ast_global('*i'),
               ast_binop(pj_binop_range, ast_constant(0), ast_constant(9)),
               ast_statementsequence([
                   ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_global('$i')),
               ]),
             ));
ast_contains(sub { for (0..9) { my $j = $_ } },
             ast_foreach(
               ast_global('*_'),
               ast_binop(pj_binop_range, ast_constant(0), ast_constant(9)),
               ast_statementsequence([
                   ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_global('$_')),
               ]),
             ));
ast_contains(sub { for (0..9) { 1 } },
             ast_foreach(
               ast_global('*_'),
               ast_binop(pj_binop_range, ast_constant(0), ast_constant(9)),
               ast_empty(),
             ));
ast_contains(sub { my $j = $_ for 0..9 },
             ast_foreach(
               ast_global('*_'),
               ast_binop(pj_binop_range, ast_constant(0), ast_constant(9)),
               ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_global('$_')),
             ));
ast_contains(sub { our @x; my $j = $_ for @x },
             ast_foreach(
               ast_global('*_'),
               ast_global('@x'),
               ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_global('$_')),
             ));
ast_contains(sub { our @x; my $j = $_ for reverse @x },
             ast_foreach(
               ast_global('*_'),
               ast_listop(pj_listop_reverse, [
                 ast_global('@x'),
               ]),
               ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_global('$_')),
             ));
ast_contains(sub { our (@x, @y); my $j = $_ for @x, @y },
             ast_foreach(
               ast_global('*_'),
               ast_list(
                 ast_global('@x'),
                 ast_global('@y'),
               ),
               ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_global('$_')),
             ));
ast_contains(sub { our $x; my $j = $_ for $x },
             ast_foreach(
               ast_global('*_'),
               ast_list(
                 ast_global('$x'),
               ),
               ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_global('$_')),
             ));

done_testing();
