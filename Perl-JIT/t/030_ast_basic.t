#!/usr/bin/env perl

use t::lib::Perl::JIT::ASTTest;

# this is more a test for ASTTest than a test for Perl::JIT...
ast_contains(sub { my $a; ++$a },
             ast_unop(pj_unop_preinc, ast_lexical('$a')));
ast_contains(sub { my $a; 1 + $a },
             ast_binop(pj_binop_add, ast_constant(1), ast_lexical('$a')));
ast_contains(sub { return 1, 2, 3 },
             ast_listop(pj_listop_return, [
               ast_constant(1),
               ast_constant(2),
               ast_constant(3),
             ]));
ast_contains(sub { my $a = 2; ++$a; return; },
             ast_statementsequence([
               ast_binop(pj_binop_sassign, ast_lexical('$a'), ast_constant(2)),
               ast_unop(pj_unop_preinc, ast_lexical('$a')),
               ast_anything,
             ]));
ast_contains(sub { my $a; my @a; my %h; },
             ast_statementsequence([
               ast_lexical('$a'),
               ast_lexical('@a'),
               ast_lexical('%a'),
             ]));
ast_contains(sub { my ($a, $b); print $a, $b; },
             ast_listop(pj_listop_print, [
               ast_lexical('$a'),
               ast_lexical('$b'),
             ]));

done_testing();
