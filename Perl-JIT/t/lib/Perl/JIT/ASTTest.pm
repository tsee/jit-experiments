package # hide from PAUSE
  Perl::JIT::ASTTest;
use v5.14;
use warnings;
# This is to make given/when work on 5.14 and 5.18. *sigh*, and it
# needs to be after 'use Moo'
no warnings $] < 5.018 ? 'redefine' : 'experimental';
use warnings 'redefine';

use Exporter 'import';
use Carp qw(croak);

use Perl::JIT qw(:all);
use Test::Simple;

our @EXPORT = (qw(
  ast_anything
  ast_empty
  ast_constant
  ast_lexical
  ast_global
  ast_statement
  ast_statementsequence
  ast_bareblock
  ast_while
  ast_until
  ast_for
  ast_foreach
  ast_map
  ast_grep
  ast_baseop
  ast_unop
  ast_binop
  ast_listop
  ast_block
  ast_list
  ast_subcall
  ast_methodcall
  ast_next
  ast_redo
  ast_last

  ast_contains
), @Perl::JIT::EXPORT_OK);

sub _matches {
  my ($ast, $pattern) = @_;

  given ($pattern->{type}) {
    when ('anything') { return 1 }
    when ('empty') { return $ast->get_type == pj_ttype_empty }
    when ('constant') {
      return 0 unless $ast->get_type == pj_ttype_constant;
      return 0 if defined $pattern->{value_type} &&
                  $ast->const_type != $pattern->{value_type};

      given ($ast->const_type) {
        when (pj_double_type) {
          return $ast->get_dbl_value == $pattern->{value};
        }
        when (pj_int_type) {
          return $ast->get_int_value == $pattern->{value};
        }
        when (pj_uint_type) {
          return $ast->get_uint_value == $pattern->{value};
        }
        when (pj_unspecified_type) {
          return !defined $pattern->{value};
        }
        default {
          die "Unhandled constant type in pattern" ;
        }
      }
    }
    when ('lexical') {
      return 0 unless $ast->get_type == pj_ttype_lexical || $ast->get_type == pj_ttype_variabledeclaration;
      return 0 unless $ast->get_sigil == $pattern->{sigil};

      return 1;
    }
    when ('global') {
      return 0 unless $ast->get_type == pj_ttype_global;
      return 0 unless $ast->get_sigil == $pattern->{sigil};

      return 1;
    }
    when ('statement') {
      return 0 unless $ast->get_type == pj_ttype_statement;
      return _matches($ast->get_kid, $pattern->{term});
    }
    when ('statementsequence') {
      return 0 unless $ast->get_type == pj_ttype_statementsequence;
      my @kids = $ast->get_kids;
      return 0 unless @kids == @{$pattern->{statements}};

      for my $i (0 .. $#kids) {
        return 0 if !_matches($kids[$i], $pattern->{statements}[$i]);
      }

      return 1;
    }
    when ('bareblock') {
      return 0 unless $ast->get_type == pj_ttype_bareblock;
      return _matches($ast->get_body, $pattern->{body}) &&
             (!defined $pattern->{continuation} ||
              _matches($ast->get_continuation, $pattern->{continuation}));
    }
    when ('while') {
      return 0 unless $ast->get_type == pj_ttype_while;
      return 0 unless $ast->get_negated == $pattern->{negated};
      return _matches($ast->get_condition, $pattern->{condition}) &&
             _matches($ast->get_body, $pattern->{body}) &&
             (!defined $pattern->{continuation} ||
              _matches($ast->get_continuation, $pattern->{continuation}));
    }
    when ('for') {
      return 0 unless $ast->get_type == pj_ttype_for;
      return _matches($ast->get_init, $pattern->{init}) &&
             _matches($ast->get_condition, $pattern->{condition}) &&
             _matches($ast->get_step, $pattern->{step}) &&
             _matches($ast->get_body, $pattern->{body});
    }
    when ('foreach') {
      return 0 unless $ast->get_type == pj_ttype_foreach;
      return _matches($ast->get_iterator, $pattern->{iterator}) &&
             _matches($ast->get_expression, $pattern->{expression}) &&
             _matches($ast->get_body, $pattern->{body}) &&
             (!defined $pattern->{continuation} ||
              _matches($ast->get_continuation, $pattern->{continuation}));
    }
    when ('map') {
      return 0 unless $ast->get_type == pj_ttype_map;
      return _matches($ast->get_body, $pattern->{body}) &&
             _matches($ast->get_parameters, $pattern->{parameters});
    }
    when ('grep') {
      return 0 unless $ast->get_type == pj_ttype_grep;
      return _matches($ast->get_body, $pattern->{body}) &&
             _matches($ast->get_parameters, $pattern->{parameters});
    }
    when ('baseop') {
      return 0 unless $ast->get_type == pj_ttype_op;
      return 0 unless $ast->op_class == pj_opc_baseop;
      return 0 unless $ast->get_op_type == $pattern->{op};
      return 1;
    }
    when ('unop') {
      return 0 unless $ast->get_type == pj_ttype_op;
      return 0 unless $ast->op_class == pj_opc_unop;
      return 0 unless $ast->get_op_type == $pattern->{op};

      return _matches($ast->get_kid, $pattern->{term});
    }
    when ('binop') {
      return 0 unless $ast->get_type == pj_ttype_op;
      return 0 unless $ast->op_class == pj_opc_binop;
      return 0 unless $ast->get_op_type == $pattern->{op};

      return _matches($ast->get_left_kid, $pattern->{left}) &&
             _matches($ast->get_right_kid, $pattern->{right});
    }
    when ('listop') {
      return 0 unless $ast->get_type == pj_ttype_op;
      return 0 unless $ast->op_class == pj_opc_listop;
      return 0 unless $ast->get_op_type == $pattern->{op};
      my @kids = $ast->get_kids;
      return 0 unless ref($pattern->{terms}) eq "ARRAY";
      return 0 unless @kids == @{$pattern->{terms}};

      for my $i (0 .. $#kids) {
        return 0 if !_matches($kids[$i], $pattern->{terms}[$i]);
      }

      return 1;
    }
    when ('block') {
      return 0 unless $ast->get_type == pj_ttype_op;
      return 0 unless $ast->op_class == pj_opc_block;
      my @kids = $ast->get_kids;
      die "Blocks can only have one kid" if @kids != 1;
      return _matches($kids[0], $pattern->{body});
    }
    when ('list') {
      return 0 unless $ast->get_type == pj_ttype_list;
      my @kids = $ast->get_kids;
      return 0 unless @kids == @{$pattern->{terms}};

      for my $i (0 .. $#kids) {
        return 0 if !_matches($kids[$i], $pattern->{terms}[$i]);
      }

      return 1;
    }
    when ('call') {
      return 0 unless $ast->get_type == pj_ttype_function_call;

      my $invocant = $pattern->{invocant};
      if ($invocant) {
        return 0 if not $ast->isa("Perl::JIT::AST::MethodCall");
        return 0 if !_matches($ast->get_invocant(), $invocant);
      }
      else {
        return 0 if not $ast->isa("Perl::JIT::AST::SubCall");
      }

      return 0 if !_matches($ast->get_cv_source(), $pattern->{cv_source});

      my @args = $ast->get_arguments;
      return 0 if @args != @{$pattern->{args}};


      for my $i (0 .. $#args) {
        return 0 if !_matches($args[$i], $pattern->{args}[$i]);
      }

      return 1;
    }
    when ('next') {
      return 0 unless $ast->get_type == pj_ttype_loop_control;
      return 0 unless $ast->get_loop_ctl_type == pj_lctl_next;
      return 0 unless $ast->get_label eq $pattern->{label};
      if ($pattern->{kid}) {
        return 0 if !_matches(($ast->get_kids())[0], $pattern->{kid});
      }
      else {
        return 0 if scalar($ast->get_kids);
      }

      

      return 1;
    }
    when ('redo') {
      return 0 unless $ast->get_type == pj_ttype_loop_control;
      return 0 unless $ast->get_loop_ctl_type == pj_lctl_redo;
      return 0 unless $ast->get_label eq $pattern->{label};
      if ($pattern->{kid}) {
        return 0 if !_matches(($ast->get_kids())[0], $pattern->{kid});
      }
      else {
        return 0 if scalar($ast->get_kids);
      }

      return 1;
    }
    when ('last') {
      return 0 unless $ast->get_type == pj_ttype_loop_control;
      return 0 unless $ast->get_loop_ctl_type == pj_lctl_last;
      return 0 unless $ast->get_label eq $pattern->{label};
      if ($pattern->{kid}) {
        return 0 if !_matches(($ast->get_kids())[0], $pattern->{kid});
      }
      else {
        return 0 if scalar($ast->get_kids);
      }

      return 1;
    }
  }
}

sub _contains {
  my ($ast, $pattern) = @_;
  my @queue = $ast;

  while (@queue) {
    my $term = shift @queue;

    return 1 if _matches($term, $pattern);
    push @queue, $term->get_kids;
  }

  return 0;
}

sub ast_contains {
  my ($sub, $pattern, $diag) = @_;
  local $Test::Builder::Level = $Test::Builder::Level + 1;

  my @asts = Perl::JIT::find_jit_candidates($sub);
  for my $ast (@asts) {
    return ok(1, $diag) if _contains($ast, $pattern);
  }

  $_->dump for @asts;
  ok(0, $diag);
}

sub ast_anything {
  return {type => 'anything'};
}

sub ast_empty {
  return {type => 'empty'}
}

sub ast_constant {
  my ($value, $type) = @_;

  return {type => 'constant', value => $value, value_type => $type};
}

sub ast_lexical {
  my ($name) = @_;
  die "Invalid name '$name'" unless $name =~ /^([\$\@\%])(.+)$/;
  my $sigil = $1 eq '$' ? pj_sigil_scalar :
              $1 eq '@' ? pj_sigil_array :
                          pj_sigil_hash;

  return {type => 'lexical', sigil => $sigil, name => $2};
}

sub ast_global {
  my ($name) = @_;
  die "Invalid name '$name'" unless $name =~ /^([\$\@\%\*])(.+)$/;
  my $sigil = $1 eq '$' ? pj_sigil_scalar :
              $1 eq '@' ? pj_sigil_array :
              $1 eq '*' ? pj_sigil_glob :
                          pj_sigil_hash;

  return {type => 'global', sigil => $sigil, name => $2};
}

sub ast_statement {
  my ($term) = @_;

  return {type => 'statement', term => $term};
}

sub _force_statements {
  my ($maybe_statements) = @_;

  return [
    map {
      $_->{type} eq 'anything' || $_->{type} eq 'statement' ? $_ : ast_statement($_)
    } @$maybe_statements
  ];
}

sub ast_statementsequence {
  my ($statements) = @_;

  return {type => 'statementsequence', statements => _force_statements($statements)};
}

sub ast_bareblock {
  my ($body, $continuation) = @_;

  return {type => 'bareblock', body => $body, continuation => $continuation};
}

sub ast_while {
  my ($condition, $body, $continuation) = @_;

  return {type => 'while', condition => $condition, negated => 0,
          body => $body, continuation => $continuation};
}

sub ast_until {
  my ($condition, $body, $continuation) = @_;

  return {type => 'while', condition => $condition, negated => 1,
          body => $body, continuation => $continuation};
}

sub ast_for {
  my ($init, $condition, $step, $body) = @_;

  return {type => 'for', init => $init, condition => $condition,
          step => $step, body => $body};
}

sub ast_foreach {
  my ($iterator, $expression, $body, $continuation) = @_;

  return {type => 'foreach', iterator => $iterator, expression => $expression,
          body => $body, continuation => $continuation};
}

sub ast_map {
  my ($body, $params) = @_;

  return {type => 'map',
          body => $body, parameters => $params};
}

sub ast_grep {
  my ($body, $params) = @_;

  return {type => 'grep',
          body => $body, parameters => $params};
}

sub ast_baseop {
  my ($op) = @_;
  croak("ast_baseop: Called with wrong number of parameters. Do you want a different class of op?")
    if @_ != 1; 

  return {type => 'baseop', op => $op};
}

sub ast_unop {
  my ($op, $term) = @_;
  croak("ast_unop: Called with wrong number of parameters. Do you want a different class of op?")
    if @_ != 2; 

  return {type => 'unop', op => $op, term => $term};
}

sub ast_binop {
  my ($op, $left, $right) = @_;
  croak("ast_binop: Called with wrong number of parameters. Do you want a different class of op?")
    if @_ != 3;

  return {type => 'binop', op => $op, left => $left, right => $right};
}

sub ast_listop {
  my ($op, $terms) = @_;

  return {type => 'listop', op => $op, terms => $terms};
}

sub ast_list {
  my (@terms) = @_;

  return {type => 'list', terms => \@terms};
}

sub ast_block {
  my ($body) = @_;

  return {type => 'block', body => $body};
}

sub ast_subcall {
  my ($cv_src, @args) = @_;

  return {type => 'call', cv_source => $cv_src, args => \@args};
}

sub ast_methodcall {
  my ($invocant, $cv_src, @args) = @_;

  return {type => 'call', cv_source => $cv_src,
          invocant => $invocant, args => \@args};
}

sub ast_next {
  my ($label, $target, $kid) = @_;

  return {type => 'next', label => $label,
          target => $target, kid => $kid};
}

sub ast_redo {
  my ($label, $target, $kid) = @_;

  return {type => 'redo', label => $label,
          target => $target, kid => $kid};
}

sub ast_last {
  my ($label, $target, $kid) = @_;

  return {type => 'last', label => $label,
          target => $target, kid => $kid};
}

package t::lib::Perl::JIT::ASTTest;

use strict;
use warnings;
use parent 'Test::Builder::Module';

use Test::More;
use Test::Differences;
use Perl::JIT;

Perl::JIT::ASTTest->import;

our @EXPORT = (
  @Test::More::EXPORT,
  @Test::Differences::EXPORT,
  @Perl::JIT::ASTTest::EXPORT,
);

sub import {
    unshift @INC, 't/lib';

    strict->import;
    warnings->import;

    goto &Test::Builder::Module::import;
}

1;
