#!/usr/bin/env perl
use t::lib::Perl::JIT::ASTTest;
use t::lib::Perl::JIT::Test;
use Test::Warn;


# TODO find a way to test whether the jump target is the right one
ast_contains(sub { while (1) { next } },
             ast_while(
               ast_empty(),
               ast_statementsequence([
                 ast_next("", ""),
               ]),
             ));

ast_contains(sub { while (1) { next; last; redo } },
             ast_while(
               ast_empty(),
               ast_statementsequence([
                 ast_next("", ""),
                 ast_last("", ""),
                 ast_redo("", ""),
               ]),
             ));

warning_like {
    ast_contains(sub { FOO: while (1) { next; last FOO; redo BAR; } },
                 ast_while(
                   ast_empty(),
                   ast_statementsequence([
                     ast_next("", ""),
                     ast_last("FOO", ""),
                     ast_redo("BAR", ""),
                   ]),
                 ));
  }
  qr/Some loop control statements/,
  "Expected warning about dangling loop control statement";

my $foo;
ast_contains(sub { FOO: while (1) { next; while(1) {last; redo FOO} } },
             ast_while(
               ast_empty(),
               ast_statementsequence([
                 ast_next("", ""),
                 ast_while(
                   ast_empty(),
                   ast_statementsequence([
                     ast_last("", ""),
                     ast_redo("FOO", ""),
                   ]),
                  ),
               ]),
             ));

done_testing();
