#!/usr/bin/env perl

use t::lib::Perl::JIT::ASTTest;

ast_contains(sub { my $a; ++$a },
             ast_unop(pj_unop_preinc, ast_lexical('$a')));
ast_contains(sub { use integer; my $a; ++$a },
             ast_unop(pj_unop_preinc, ast_lexical('$a')));

ast_contains(sub { my $a; my $b = $a * 2 },
             ast_binop(
               pj_binop_sassign,
               ast_lexical('$b'),
               ast_binop(pj_binop_multiply, ast_lexical('$a'), ast_constant(2)),
             ));
ast_contains(sub { use integer; my $a; my $b = $a * 2 },
             ast_binop(
               pj_binop_sassign,
               ast_lexical('$b'),
               ast_binop(pj_binop_multiply, ast_lexical('$a'), ast_constant(2)),
             ));

ast_contains(sub { my $a; my $b = -$a },
             ast_binop(
               pj_binop_sassign,
               ast_lexical('$b'),
               ast_unop(pj_unop_negate, ast_lexical('$a')),
             ));
ast_contains(sub { my $a; use integer; my $b = -$a },
             ast_binop(
               pj_binop_sassign,
               ast_lexical('$b'),
               ast_unop(pj_unop_negate, ast_lexical('$a')),
             ));


done_testing();
