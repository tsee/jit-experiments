#!/usr/bin/env perl

use t::lib::Perl::JIT::ASTTest;

ast_contains(sub { do { my $a } },
             ast_block(
               ast_lexical('$a')
             ));
ast_contains(sub { do { my $a = 1; my $b = 2 } },
             ast_block(
               ast_statementsequence([
                 ast_binop(pj_binop_sassign, ast_lexical('$a'), ast_constant(1)),
                 ast_binop(pj_binop_sassign, ast_lexical('$b'), ast_constant(2)),
               ])
             ));
ast_contains(sub { do { no warnings 'void'; 1; 2; 3; 4 } },
             ast_block(
               ast_statementsequence([
                 ast_constant(4)
               ])
             ));
ast_contains(sub { do { do {my $a} } },
             ast_block(
               ast_block(
                 ast_lexical('$a')
               )
             ));
ast_contains(sub { our $x; do { do {$x = 123} } },
             ast_block(
               ast_block(
                 ast_global('$x')
               )
             ));

done_testing();
