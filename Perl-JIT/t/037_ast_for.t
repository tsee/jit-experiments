#!/usr/bin/env perl

use t::lib::Perl::JIT::ASTTest;

ast_contains(sub { for (my $i = 0; $i < 10; ++$i) { my $j = $i } },
             ast_for(
               ast_binop(pj_binop_sassign, ast_lexical('$i'), ast_constant(0)),
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_unop(pj_unop_preinc, ast_lexical('$i')),
               ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_lexical('$i')),
             ));
ast_contains(sub { for (my $i = 0; ; ++$i) { my $j = $i } },
             ast_for(
               ast_binop(pj_binop_sassign, ast_lexical('$i'), ast_constant(0)),
               ast_empty(),
               ast_unop(pj_unop_preinc, ast_lexical('$i')),
               ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_lexical('$i')),
             ));
ast_contains(sub { for (my $i = 0; $i < 10;) { my $j = $i } },
             ast_for(
               ast_binop(pj_binop_sassign, ast_lexical('$i'), ast_constant(0)),
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_empty(),
               ast_statementsequence([
                   ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_lexical('$i'))
               ]),
             ));
ast_contains(sub { for (my $i = 0; $i < 10; ++$i) { } },
             ast_for(
               ast_binop(pj_binop_sassign, ast_lexical('$i'), ast_constant(0)),
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_unop(pj_unop_preinc, ast_lexical('$i')),
               ast_empty(),
             ));
ast_contains(sub { for (my $i = 0; $i < 10; ++$i) { 1 } },
             ast_for(
               ast_binop(pj_binop_sassign, ast_lexical('$i'), ast_constant(0)),
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_unop(pj_unop_preinc, ast_lexical('$i')),
               ast_empty(),
             ));
ast_contains(sub { for (my $i = 0; ; ++$i) { } },
             ast_for(
               ast_binop(pj_binop_sassign, ast_lexical('$i'), ast_constant(0)),
               ast_empty(),
               ast_unop(pj_unop_preinc, ast_lexical('$i')),
               ast_empty(),
             ));
ast_contains(sub { for (my $i = 0; $i < 10;) { } },
             ast_for(
               ast_binop(pj_binop_sassign, ast_lexical('$i'), ast_constant(0)),
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_empty(),
               ast_empty(),
             ));
ast_contains(sub { for (my $i = 0; ;) { } },
             ast_for(
               ast_binop(pj_binop_sassign, ast_lexical('$i'), ast_constant(0)),
               ast_empty(),
               ast_empty(),
               ast_empty(),
             ));
ast_contains(sub { for (my $i = 0; ;) { 1 } },
             ast_for(
               ast_binop(pj_binop_sassign, ast_lexical('$i'), ast_constant(0)),
               ast_empty(),
               ast_empty(),
               ast_empty(),
             ));
ast_contains(sub { my $i; for (; $i < 10; ++$i) { my $j = $i } },
             ast_for(
               ast_empty(),
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_unop(pj_unop_preinc, ast_lexical('$i')),
               ast_binop(pj_binop_sassign, ast_lexical('$j'), ast_lexical('$i')),
             ));
ast_contains(sub { my $i; for (; $i < 10; ++$i) { } },
             ast_for(
               ast_empty(),
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_unop(pj_unop_preinc, ast_lexical('$i')),
               ast_empty(),
             ));
ast_contains(sub { my $i; for (; $i < 10;) { ++$i } },
             ast_while(
               ast_binop(pj_binop_num_lt, ast_lexical('$i'), ast_constant(10)),
               ast_statementsequence([
                 ast_unop(pj_unop_preinc, ast_lexical('$i')),
               ]),
             ));
ast_contains(sub { my $i; for (;;) { ++$i } },
             ast_while(
               ast_empty(),
               ast_statementsequence([
                 ast_unop(pj_unop_preinc, ast_lexical('$i')),
               ]),
             ));
ast_contains(sub { my $i; for (;;) { } },
             ast_while(
               ast_empty(),
               ast_empty(),
             ));
ast_contains(sub { my $i; for (;;) {do{++$i}} },
             ast_while(
               ast_empty(),
               ast_statementsequence([
                 ast_block(
                   ast_unop(pj_unop_preinc, ast_lexical('$i')),
                 ),
               ])
             ));

done_testing();
