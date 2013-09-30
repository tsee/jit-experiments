#!/usr/bin/env perl

use t::lib::Perl::JIT::ASTTest;

ast_contains(sub { my $i; until ($i < 10) { my $j = $i } },
             ast_until(
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_statementsequence([
                 ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_lexical('$i')),
               ]),
             ));
ast_contains(sub { my $i; until ($i < 10) { my $j = $i } continue { ++$i } },
             ast_until(
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_lexical('$i')),
               ast_unop(pj_unop_preinc, ast_lexical('$i')),
             ));
ast_contains(sub { my $i; until ($i < 10) { } continue { ++$i } },
             ast_until(
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_empty(),
               ast_unop(pj_unop_preinc, ast_lexical('$i')),
             ));
ast_contains(sub { my $i; until (0) { ++$i } },
             ast_while(
               ast_empty(),
               ast_statementsequence([
                 ast_unop(pj_unop_preinc, ast_lexical('$i')),
               ])
             ));
ast_contains(sub { my $i; until (0) { } },
             ast_while(
               ast_empty(),
               ast_empty(),
             ));

done_testing();
