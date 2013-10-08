#!/usr/bin/env perl

use t::lib::Perl::JIT::ASTTest;

ast_contains(sub { my $i; { my $j = $i } },
             ast_bareblock(
               ast_statementsequence([
                 ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_lexical('$i')),
               ]),
             ));
ast_contains(sub { my $i; { my $j = $i } continue { ++$i } },
             ast_bareblock(
               ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_lexical('$i')),
               ast_unop(pj_unop_preinc, ast_lexical('$i')),
             ));
ast_contains(sub { my $i; {;} continue { ++$i } },
             ast_bareblock(
               ast_empty(),
               ast_unop(pj_unop_preinc, ast_lexical('$i')),
             ));
ast_contains(sub { {;}; my $b },
             ast_statementsequence([
               ast_empty(),
               ast_lexical('$b'),
             ]));

done_testing();
