#!/usr/bin/env perl

BEGIN {
  if ($] < 5.018000) {
    require Test::More;
    Test::More->import(skip_all=>"given/when only on 5.18.X and better");
    exit(0);
  }
}

use 5.018;
use t::lib::Perl::JIT::ASTTest;
use t::lib::Perl::JIT::Test;

my $x;
my $y;

no warnings; # no warnings 'experimental' doesn't work?

my $postinc = ast_unop(pj_unop_postinc, ast_lexical('$y'));
my $const_smartmatch = ast_binop(pj_binop_smartmatch, ast_global('$_'), ast_constant(2));

ast_contains(sub { given ($x) {$y++} },
              ast_binop(
                pj_binop_given,
                ast_lexical('$x'),
                ast_block($postinc)
              ));

ast_contains(sub { given ($x) {when(2) {$y++}} },
              ast_binop(
                pj_binop_given,
                ast_lexical('$x'),
                ast_block(
                  ast_binop(
                    pj_binop_when,
                    $const_smartmatch,
                    ast_block($postinc)
                  )
                )
              ));

TODO: {
  local $TODO = 'this fails on the statement modifier form of when. Specifically, pj_build_ast() makes an "Empty" out of the OP_SCOPE around $y++';
  ast_contains(sub { given ($x) {when(2) {$y++} $y++ when(1);} },
                ast_binop(
                  pj_binop_given,
                  ast_lexical('$x'),
                  ast_block(
                    ast_binop(
                      pj_binop_when,
                      $const_smartmatch,
                      ast_block($postinc)
                    ),
                    ast_binop(
                      pj_binop_when,
                      $const_smartmatch,
                      $postinc
                    ),
                  )
                ));
}

ast_contains(sub { given ($x) {when(2) {$y++} default {$y++}} },
              ast_binop(
                pj_binop_given,
                ast_lexical('$x'),
                ast_block(
                  ast_statementsequence([
                    ast_binop(
                      pj_binop_when,
                      $const_smartmatch,
                      ast_block($postinc)
                    ),
                    ast_unop(
                      pj_unop_default,
                      ast_block($postinc)
                    )
                  ])
                )
              ));

# TODO break
# TODO continue

done_testing();
