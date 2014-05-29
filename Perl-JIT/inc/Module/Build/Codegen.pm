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
    my ($matcher_class, $emitter_class, $matcher_header, $matcher_source, $emitter_source) = @_;

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
#define FUNCTOR_CODEGEN(node, functor_id, arity, extra_arity) \
  PerlJIT::map_codegen_functor(node, functor_id, arity, extra_arity)

EOT
    $burs->add_rules_header(<<'EOT');
#include "pj_emitter.h"
#include <stdio.h>

using namespace PerlJIT;
using namespace PerlJIT::AST;

EOT
    $burs->set_node_type('PerlJIT::CodegenNode');
    $burs->set_result_type('PerlJIT::EmitValue');
    $burs->set_root_label('RootExpr');
    $burs->set_default_label('RootExpr');

    _add_rules($burs, _parse_rules('src/emitter.txt'));

    my $source = $burs->generate($matcher_class, $emitter_class, $matcher_header, $matcher_source, $emitter_source);
    replace_if_changed($matcher_header, $source->{matcher_header});
    replace_if_changed($matcher_source, $source->{matcher_source});
    replace_if_changed($emitter_source, $source->{emitter_source});
}

sub _format_code {
    my ($code, $args) = @_;

    $code =~ s{\$\$}{state->result}g;
    for my $name (keys %$args) {
        my $pos = $args->{$name};
        my $arg = 'state' . join('->', map { "->args[$_]" } @$pos);
        my $in = $arg . '->node';
        my $out = $arg . '->result';

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
    my (@rules, %names, %constants, $rule);

    local $_ = $text;
    s{^#.*\n}{\n}mg;
    while (length $_) {
        s{\A\s+}{};
        s{\A(\w+):\s*}{} or die "Can't find tag in string '$_'";
        my $tag = $1;
        s{\A(.*?)(?=\z|^\w+:)}{}ms or die "Can't parse value in string '$_'";
        my $value = $1;

        if ($tag eq 'name') {
            $value =~ m{^(\w+)\s*} or die "Invalid rule name '$value'";
            $rule = { name => $1, tags => {} };
            push @rules, $rule;
            $names{$1} = 1;
        } elsif ($tag eq 'match' || $tag eq 'delay' || $tag eq 'emit_llvm') {
            push @{$rule->{tags}{$tag}}, $value;
        } else {
            die "Unhandled tag '$tag'";
        }
    }

    for my $rule (@rules) {
        my $tags = delete $rule->{tags};
        for my $value (@{$tags->{match}}) {
            push @{$rule->{match}}, _parse_match(\%names, \%constants, $value);
        }
        if ($tags->{delay}) {
            die "Multiple 'delay' tags in rule" if @{$tags->{delay}} > 1;
            $rule->{delay} = _parse_delay($tags->{delay}->[0]);
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
    my ($names, $constants, $value) = @_;
    my (@rules, @consts, %args);

    if ($value =~ m{^\s*(\w+)\s*$} && exists $names->{$1}) {
        return {
            rule => $1,
        };
    }

    $rules[0] = [];
    my $orig_value = $value;
    while (length($value)) {
        $value =~ s{^\s+}{};

        if ($value =~ s{^\@\s+(\$\w+)\s*}{}) {
            $args{$1} = [map $#{$rules[$_]}, 0..$#rules - 1];
        } elsif ($value =~ s{^\s*(\w+)\s*}{}) {
            my $name = $1;
            if (exists $names->{$name}) {
                $rules[-1] = $name;
            } else {
                if ($name =~ /^pj_/) {
                    $constants->{$name} ||= "Const_$name";
                    $name = "Const_$name";
                }
                $rules[-1][0] = $name;

                if ($value =~ s{^\(\s*}{}) {
                    push @rules, [];
                }
            }
        } elsif ($value =~ s{^\)\s*}{}) {
            die "Too many closed parentheses" if @rules == 1;
            my $rule = pop @rules;
            push @{$rules[-1]}, $rule;
        } elsif ($value =~ s{^,\s*}{}) {
            my $rule = pop @rules;
            push @{$rules[-1]}, $rule;
            push @rules, [];
        } else {
            die "Error parsing match '$orig_value' near '$value'";
        }
    }
    die "Missing closing parenthesis" if @rules > 1;

    return {
        rule    => $rules[0],
        args    => \%args,
    }
}

sub _parse_delay {
    my ($value) = @_;
    my @args = split /\s*,\s*/, $value;

    # TODO check syntax sanity

    return \@args;
}

sub _add_rules {
    my ($burs, $rules, $constants) = @_;

    for my $const (keys %$constants) {
        $burs->define_functor($constants->{$const}, $const);
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
}

1;
