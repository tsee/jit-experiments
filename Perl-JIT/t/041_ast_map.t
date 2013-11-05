#!/usr/bin/env perl

use t::lib::Perl::JIT::ASTTest;

ast_contains(sub { my @a = map 1, 2; },
             ast_map(
               ast_constant(1),
               ast_list(ast_constant(2)),
             ));

ast_contains(sub { my @a = map $_+1, (1, 2); },
             ast_map(
               ast_binop(pj_binop_add, ast_global('$_'), ast_constant(1)),
               ast_list(ast_constant(1), ast_constant(2)),
             ));

ast_contains(sub { my @a = map {$_} (1, 2); },
             ast_map(
               ast_block( ast_global('$_') ),
               ast_list(ast_constant(1), ast_constant(2)),
             ));

ast_contains(sub { my @a = map {my $x = $_+1; $x} (1, 2); },
             ast_map(
               ast_block(
                 ast_statementsequence([
                   ast_statement(
                     ast_binop(
                       pj_binop_sassign,
                       ast_lexical('$x'),
                       ast_binop(pj_binop_add, ast_global('$_'), ast_constant(1))
                     )
                   ),
                   ast_statement(ast_lexical('$x'))
                 ])
               ),
               ast_list(ast_constant(1), ast_constant(2)),
             ));

done_testing();
