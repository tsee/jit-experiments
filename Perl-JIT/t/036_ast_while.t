#!/usr/bin/env perl

use t::lib::Perl::JIT::ASTTest;

ast_contains(sub { my $i; while ($i < 10) { my $j = $i } },
             ast_while(
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_statementsequence([
                 ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_lexical('$i')),
               ]),
             ));
ast_contains(sub { my $i; while ($i < 10) { my $j = $i } continue { ++$i } },
             ast_while(
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_lexical('$i')),
               ast_unop(pj_unop_preinc, ast_lexical('$i')),
             ));
ast_contains(sub { my $i; while ($i < 10) { my $j = $i; ++$j } },
             ast_while(
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_statementsequence([
                 ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_lexical('$i')),
                 ast_unop(pj_unop_preinc, ast_lexical('$j')),
               ]),
             ));
ast_contains(sub { my $i; while ($i < 10) { } },
             ast_while(
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_empty(),
             ));
ast_contains(sub { my $i; while ($i < 10) { } continue { ++$i } },
             ast_while(
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_empty(),
               ast_unop(pj_unop_preinc, ast_lexical('$i')),
             ));
ast_contains(sub { my $i; while (1) { ++$i } },
             ast_while(
               ast_empty(),
               ast_statementsequence([
                 ast_unop(pj_unop_preinc, ast_lexical('$i')),
               ]),
             ));
ast_contains(sub { my $i; while (1) { } },
             ast_while(
               ast_empty(),
               ast_empty(),
             ));
ast_contains(sub { my $i; while (1) { } continue { } },
             ast_while(
               ast_empty(),
               ast_empty(),
               ast_empty(),
             ));

done_testing();
