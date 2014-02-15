#!/usr/bin/env perl

use t::lib::Perl::JIT::ASTTest;

diag("Still TODO: rv2cv, pass-through ex-rv2cv, then fix subcall tests");

my ($x, $y, $foo, $bar);

ast_contains(sub { foo($x, $y) },
             ast_subcall(
               ast_anything(),
               ast_lexical('$x'), ast_lexical('$y')
             ));

ast_contains(sub { $foo->($x, $y) },
             ast_subcall(
               ast_lexical('$foo'),
               ast_lexical('$x'), ast_lexical('$y')
             ));

ast_contains(sub { &$foo($x, $y) },
             ast_subcall(
               ast_lexical('$foo'),
               ast_lexical('$x'), ast_lexical('$y')
             ));

ast_contains(sub { $foo->bar($x, $y) },
             ast_methodcall(
               ast_lexical('$foo'),
               ast_baseop(pj_baseop_method_named),
               ast_lexical('$x'), ast_lexical('$y')
             ));

ast_contains(sub { $foo->$bar($x, $y) },
             ast_methodcall(
               ast_lexical('$foo'),
               ast_unop(pj_unop_method, ast_lexical('$bar')),
               ast_lexical('$x'), ast_lexical('$y')
             ));

done_testing();
