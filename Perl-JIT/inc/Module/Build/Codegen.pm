package Module::Build::Codegen;
use 5.14.2;
use strict;
use warnings;

use Exporter qw(import);

use Algorithm::Burs::Cpp;
use Module::Build::Utils qw(replace_if_changed);

our @EXPORT_OK = qw(create_codegen_class);
our %EXPORT_TAGS = ( all => \@EXPORT_OK );

sub create_codegen_class {
    my ($matcher_class, $emitter_class, $matcher_header, $matcher_source, $emitter_source, $ast_map_inc) = @_;

    my $burs = Algorithm::Burs::Cpp->new(
        functor_ordinal => 1000,
        tag_ordinal     => 1000,
    );

    $burs->add_interface_header(<<'EOT');
#include "pj_ast_map.h"
#include "pj_emitvalue.h"

EOT
    $burs->add_implementation_header(<<'EOT');
#define ARG_CODEGEN(node, index) \
  PerlJIT::map_codegen_arg(node, functor_id, index)
#define FUNCTOR_CODEGEN(node) \
  PerlJIT::map_codegen_functor(node)

EOT
    $burs->add_rules_header(<<'EOT');
#include "pj_emitter.h"
#include <stdio.h>

using namespace PerlJIT;
using namespace PerlJIT::AST;

EOT
    $burs->set_node_type('PerlJIT::CodegenNode');
    $burs->set_mapped_functor_type('PerlJIT::MappedFunctor');
    $burs->set_result_type('PerlJIT::EmitValue');
    $burs->set_root_label('RootExpr');
    $burs->set_default_label('RootExpr');

    my $ast_map_ops = _add_rules($burs, _parse_rules('src/emitter.txt'));

    my $source = $burs->generate($matcher_class, $emitter_class, $matcher_header, $matcher_source, $emitter_source);
    replace_if_changed($matcher_header, $source->{matcher_header});
    replace_if_changed($matcher_source, $source->{matcher_source});
    replace_if_changed($emitter_source, $source->{emitter_source});
    replace_if_changed($ast_map_inc, $ast_map_ops);
}

sub _format_code {
    my ($code, $args) = @_;

    $code =~ s{\$\$}{state->result}g;
    for my $name (keys %$args) {
        my $pos = $args->{$name};
        my $arg = 'state' . join('->', map { "->args[$_]" } @$pos);
        my $in = $arg . '->node';
        my $out = $arg . '->result';

        $code =~ s{__\(\s*\Q$name\E\s*\)}{$arg}g;
        $code =~ s{_\(\s*\Q$name\E\s*\)}{$out}g;
        $code =~ s{\Q$name\E}{$in}g;
    }

    return $code;
}

sub _parse_rules {
    my ($file) = @_;
    my $text = do {
        open my $fh, '<', $file or die "Unable to open '$file': $!";
        local $/;
        readline $fh;
    };
    my (@rules, %names, %constants, %defines, $rule);
    my $defines_rx = '(?!a)a'; # match nothing

    local $_ = $text;
    s{^#.*\n}{\n}mg;
    while (length $_) {
        s{\A\s+}{};
        s{\A(\w+):\s*}{} or die "Can't find tag in string '$_'";
        my $tag = $1;
        s{\A(.*?)(?=\z|^\w+:)}{}ms or die "Can't parse value in string '$_'";
        my $value = $1;

        $value =~ s{\s+\z}{};

        if ($tag eq 'name') {
            $value =~ m{^(\w+)$} or die "Invalid rule name '$value'";
            $rule = { name => $1, tags => {} };
            push @rules, $rule;
            $names{$1} = 1;
        } elsif ($tag eq 'delay' || $tag eq 'emit_llvm' || $tag eq 'weight') {
            push @{$rule->{tags}{$tag}}, $value;
        } elsif ($tag eq 'match') {
            $value =~ s{$defines_rx}{$defines{$1}}eg;

            if (index($value, '|') == -1) {
                push @{$rule->{tags}{$tag}}, $value;
            } else {
                my (@queue, @values) = $value;

                while (@queue) {
                    my $item = pop @queue;

                    $item =~ m{^(.*?)\b(\w+(?:\|\w+)+)\b(.*)$} or die "'$item' does not contain alternations";
                    my @expanded = map "$1$_$3", split /\|/, $2;
                    if (index($1, '|') == -1 && index($3, '|') == -1) {
                        push @values, @expanded;
                    } else {
                        push @queue, @expanded;
                    }
                }

                push @{$rule->{tags}{$tag}}, @values;
            }
        } elsif ($tag eq 'define') {
            die "Invalid define tag '$value'" unless $value =~ /^(\w+)\s+(.+)$/m;
            $defines{$1} = $2;

            my @sorted_defines = sort {
                length($b) - length($a)
            } keys %defines;

            $defines_rx = '\b(' . join('|', @sorted_defines) . ')\b';
        } else {
            die "Unhandled tag '$tag'";
        }
    }

    for my $rule (@rules) {
        my $tags = delete $rule->{tags};
        if ($tags->{delay}) {
            die "Multiple 'delay' tags in rule" if @{$tags->{delay}} > 1;
            $rule->{delay} = _parse_delay($tags->{delay}->[0]);
        }
        for my $value (@{$tags->{match}}) {
            push @{$rule->{match}}, _parse_match(\%names, \%constants, $rule->{delay}, $value);
        }
        if ($tags->{weight}) {
            die "Multiple 'weight' tags in rule" if @{$tags->{weight}} > 1;
            die "Weight must be a non-negative integer" unless $tags->{weight}[0] =~ /^[0-9]+$/;
        }
        if ($tags->{emit_llvm}) {
            die "Multiple 'emit_llvm' tags in rule" if @{$tags->{emit_llvm}} > 1;
            my $value = $tags->{emit_llvm}->[0];
            $value =~ s[^{\s*][] or die "Missing opening curly bracket for code";
            $value =~ s[\s*}\s*$][] or die "Missing closing curly bracket for code";

            $rule->{llvm_code} = $value;
        }
    }

    # print Data::Dumper::Dumper(\@rules);
    return (\@rules, \%constants);
}

sub _parse_match {
    my ($names, $constants, $delay, $value) = @_;
    my (@rules, @consts, %args);

    if ($value =~ m{^\s*(\w+)\s*$} && exists $names->{$1}) {
        return {
            rule => { delay => 0, match => $1 },
        };
    }

    $rules[0] = { delay => 0, match => [] };
    my $orig_value = $value;
    while (length($value)) {
        $value =~ s{^\s+}{};

        if ($value =~ s{^\@\s+(\$\w+)\s*}{}) {
            $args{$1} = [map $#{$rules[$_]{match}}, 0..$#rules - 1];
        } elsif ($value =~ s{^\s*(\w+)\s*}{}) {
            my $name = $1;
            if (exists $names->{$name}) {
                $rules[-1] = { delay => 0, match => $name };
            } else {
                if ($name =~ /^pj_/) {
                    $constants->{$name} ||= "Const_$name";
                    $name = "Const_$name";
                }
                $rules[-1]{match}[0] = $name;

                if ($value =~ s{^\(\s*}{}) {
                    push @rules, { delay => 0, match => [] };
                }
            }
        } elsif ($value =~ s{^\)\s*}{}) {
            die "Too many closed parentheses" if @rules == 1;
            my $rule = pop @rules;
            push @{$rules[-1]{match}}, $rule;
        } elsif ($value =~ s{^,\s*}{}) {
            my $rule = pop @rules;
            push @{$rules[-1]{match}}, $rule;
            push @rules, { delay => 0, match => [] };
        } else {
            die "Error parsing match '$orig_value' near '$value'";
        }
    }
    die "Missing closing parenthesis" if @rules > 1;

    for my $arg (keys %args) {
        next unless $delay && ($delay->{'*'} || $delay->{$arg});
        my $path = $args{$arg};
        next unless @$path; # root expression is always delayed
        my $expr = $rules[0];
        for my $index (@$path) {
            $expr = $expr->{match}[$index + 1];
        }
        $expr->{delay} = 1;
    }

    return {
        rule    => $rules[0],
        args    => \%args,
    }
}

sub _parse_delay {
    my ($value) = @_;
    my @args = split /\s*,\s*/, $value;

    return { '*' => 1 } if grep $_ eq '*', @args;

    for my $arg (@args) {
        die "Invalid delayed variable '$arg'" unless $arg =~ /^\$\w+/;
    }

    return { map { $_ => 1 } @args };
}

sub _add_rules {
    my ($burs, $rules, $constants) = @_;
    my (@unops, @binops, $ast_map_ops);

    for my $const (sort keys %$constants) {
        $burs->define_functor($constants->{$const}, $const);

        push @unops, $const if $const =~ /^pj_unop_/;
        push @binops, $const if $const =~ /^pj_binop_/;
    }

    if (@unops) {
        $ast_map_ops .= join "\n",
            (map "      case $_:", @unops),
                 "        return MappedFunctor(Codegen::AstUnop, 2);\n";
    }
    if (@binops) {
        $ast_map_ops .= join "\n",
            (map "      case $_:", @binops),
                 "        return MappedFunctor(Codegen::AstBinop, 3);\n";
    }

    for my $rule (@$rules) {
        for my $match (@{$rule->{match}}) {
            my $code;

            if ($rule->{llvm_code}) {
                $code = _format_code($rule->{llvm_code}, $match->{args});
            }

            $burs->add_rule(
                $rule->{weigth} || 0,
                $code,
                $rule->{name},
                $match->{rule},
            );
        }
    }

    return $ast_map_ops;
}

1;
