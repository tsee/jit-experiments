#!/usr/bin/env perl

use t::lib::Perl::JIT::ASTTest;

# This file holds a collection of (failing) tests for corner cases that need fixing.

TODO: {
  local $TODO = "Corner cases still need fixing";

  # Corner case: stringify OP
  ast_contains(sub { my $a; die "$a" },
               ast_listop(
                 pj_listop_die,
                 ast_constant(1), # FIXME how should this look?
               ));

  # Corner case: ex-stringify NULL OP:
  # B::Concise::compile(CODE(0x10a7de8))
  # 7  <1> leavesub[1 ref] K/REFC,1 ->(end)
  # -     <@> lineseq KP ->7
  # 1        <;> nextstate(main 2 (eval 13):1) v:%,{,2048 ->2
  # 6        <@> die[t4] sK/1 ->7
  # 2           <0> pushmark s ->3
  # -           <1> ex-stringify sK/1 ->6
  # -              <0> ex-pushmark s ->3
  # 5              <2> concat[t2] sK/2 ->6
  # 3                 <$> const[PV "a"] s ->4
  # -                 <1> ex-rv2sv sK/1 ->5
  # 4                    <#> gvsv[*a] s ->5
  ast_contains(sub { my $a; die "a$a" },
               ast_listop(
                 pj_listop_die,
                 ast_binop(
                   pj_binop_concat,
                   ast_constant("a"),
                   ast_lexical('$a')
                 )
               ));

}
done_testing();
