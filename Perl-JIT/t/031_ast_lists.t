#!/usr/bin/env perl

use t::lib::Perl::JIT::ASTTest;

ast_contains(sub { () },
             ast_baseop(pj_baseop_empty)
             );
ast_contains(sub { (); 1 },
             ast_baseop(pj_baseop_empty)
             );
ast_contains(sub { (1, (), (2, 3)) },
             ast_listop(pj_listop_list2scalar, [
               ast_constant(1),
               ast_listop(pj_listop_list2scalar, [
                 ast_constant(2),
                 ast_constant(3),
               ]),
             ]));
ast_contains(sub { my $a = () },
             ast_binop(pj_binop_sassign,
               ast_lexical('$a'),
               ast_constant(undef),
             ));
ast_contains(sub { my $a = ((), ()) },
             ast_binop(pj_binop_sassign,
               ast_lexical('$a'),
               ast_constant(undef),
             ));

done_testing();
