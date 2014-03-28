package Module::Build::LLVM;
use 5.14.2;
use strict;
use warnings;

use Exporter qw(import);

use Capture::Tiny qw(capture);
use File::Temp qw(tempfile);
use File::Basename qw(basename);
use Config;
use ExtUtils::Embed;
use autodie qw(open close);

our @EXPORT_OK = qw(create_function_declaration create_emitter_class);
our %EXPORT_TAGS = ( all => \@EXPORT_OK );

my $prefix = "pj_extracted_llvm_function";
my $ccopts;
my $ccopts_no_dashg;
my $clang = $ENV{CLANG} || 'clang';
my $llc = $ENV{LLVM_LLC} || 'llc';

{
    local $0 = "perl"; # allows testing with one-liners
    $ccopts = $Config{optimize} . ' ' . ExtUtils::Embed::ccopts();
    ($ccopts_no_dashg = $ccopts) =~ s/(^|\s)-g\S*(\s|$)/$1$2/g;
}

sub create_function_declaration {
    my ($prototype) = @_;
    my ($ret, $name, $proto) =
        map { s/^\s+//; s/\s+$//; tr/ \t/  /s; $_ }
            $prototype =~ /^(.*?)(\w+)\((.*)\)\s*$/
                or die "Unable to parse prototype '$prototype'";

    return {
        return => $ret,
        proto  => $proto,
        name   => $name,
    };
}

sub create_emitter_class {
    my ($prototypes, $namespaces, $class, $h_file, $c_file) = @_;
    my $creation_code = _get_creation_code($prototypes);
    my ($h_code, $c_code);
    my $h_basename = basename($h_file);

    # header file
    {
        my $h_guard = uc($h_basename =~ s/[^\w]/_/r) . '_';
        $h_code .= <<EOT;
#ifndef $h_guard
#define $h_guard

// THIS FILE WAS GENERATED. DO NOT EDIT. SEE Perl::JIT's Module::Build::LLVM.

#include <EXTERN.h>
#include <perl.h>

#include "pj_snippets.h"
EOT

        for my $namespace (@$namespaces) {
            $h_code .= <<EOT
namespace $namespace {
EOT
        }

        $h_code .= <<EOT;
  class $class : public PerlJIT::Snippets {
  public:
    $class(llvm::Module *module, llvm::IRBuilder<> *builder);

EOT

        # emitter functions declarations
        for my $function (sort keys %{$creation_code->{functions}}) {
            my $f = $creation_code->{functions}{$function};
            $h_code .= <<EOT;
    $f->{llvm_return} $function($f->{llvm_proto});
EOT
        }

        # used in emitter functions (mostly llvm::Type objects)
        $h_code .= <<EOT;
  private:
EOT

        for my $member (sort keys %{$creation_code->{members}}) {
            $h_code .= <<EOT;
    llvm::$creation_code->{members}{$member} $member;
EOT
        }

        $h_code .= <<EOT;
  };
EOT
        for my $namespace (@$namespaces) {
            $h_code .= <<EOT
}
EOT
        }

        $h_code .= <<EOT;
#endif // $h_guard
EOT
    }

    # source file
    {
        $c_code .= <<EOT;
#include "$h_basename"

// THIS FILE WAS GENERATED. DO NOT EDIT. SEE Perl::JIT's Module::Build::LLVM.

#undef _ // defined by Perl headers

$creation_code->{header}
using namespace PerlJIT;

$class\::$class(llvm::Module *module, llvm::IRBuilder<> *builder) :
  Snippets(module, builder)
{
$creation_code->{initialization}
}
EOT

        # emitter functions definitions
        for my $function (sort keys %{$creation_code->{functions}}) {
            my $f = $creation_code->{functions}{$function};
            $c_code .= <<EOT;
$f->{llvm_return}
$class\::$function($f->{llvm_proto})
  $f->{code}
EOT
        }
    }

    _replace_if_changed($h_file, $h_code);
    _replace_if_changed($c_file, $c_code);
}

sub _replace_if_changed {
    my ($path, $content) = @_;
    my $fh;

    if (-f $path) {
        local $/;
        open $fh, '<', $path;
        my $current = readline $fh;

        return if $content eq $current;
    }

    open $fh, '>', $path;
    print $fh $content;
    close $fh;
}

sub _get_creation_code {
    my ($prototypes) = @_;
    my $source = _join_source($prototypes);
    my $cpp = _run_llc(_run_clang($source));
    my ($includes) = $cpp =~ m[
        (?: ^ \s* \#include \s+ <[^<>]+> \s+ |
            ^ \s* using \s+ namespace \s+ \w+ \s* ; \s+)+
    ]xmsg or die "Unable to extract headers/namespaces";
    my ($initialization) = $cpp =~ m[
        ^Module \s* \* \s* makeLLVMModule .*? $ .*?
        (^ \s* // \s+ Type \s+ Definitions \s* $ .*? )
        (?= ^ \s+ // \s+ Function \s+ Definitions \s* $ )
    ]xmsg or die "Unable to extract initialization code";
    my @llvm_functions = $cpp =~ m[
        // \s+ Function: \s+ pj_extracted_llvm_function
        .*?
        (?= ^ \s+ (?:// \s+ Function:|return ))
    ]xmsg;

    # rename module variable
    $initialization =~ s{\bmod\b}{module}g;
    # remove function declarations for pj_extracted_llvm_function functions
    $initialization =~ s[
        ^ \s* Function \s* \* \s* \w+$prefix\_\w+
        .*?
        ^ \s* \w+$prefix\_\w+ -> setAttributes\( .*? $
    ][]gmsx or die "Could not strip $prefix* functions";
    # remove global variables that stand in for local function variables
    for my $v (qw(sp)) {
        $initialization =~ s[
            ^ [^;]* \b gvar_ptr_$v \b [^;]* ;
        ][]xmsg or die "Unable to remove global '$v' references";
    }
    # extract declarations that might become class members
    my %declarations = reverse $initialization =~ m[
        ^ \s* (\w+ \s* (?: \* | \s)) \s* (\w+) \s* (?: = | ;)
    ]gxm or die "Could not match potential member declarations";
    my $member_alt = join '|', sort { length($b) <=> length($a) }
                                    keys %declarations;
    my $member_rx = qr/$member_alt/;
    my %matched_members;

    my %functions = map { $_->{name}, { prototype => $_ } } @$prototypes;
    for my $llvm_function (@llvm_functions) {
        my $code = $llvm_function;
        my ($name) = $code =~ /\w+${prefix}_(\w+)/
            or die "Unable to match function name";
        my $fun = $functions{$name} or die "Function '$name' not found";
        # get parameter list
        my @args = $code =~ m[
            Value \s* \* \s* (\w+) \s* = \s* args \s* \+\+ \s*;
        ]gx;
        # TODO check arg count matches declaration
        # get return basic block and value
        my ($ret_block, $ret_value);
        if ($fun->{prototype}{return} eq 'void') {
            my @blocks = $code =~ m/ReturnInst::Create\([^,]+, (\w+)\);/g;
            die "No return instruction found" unless @blocks;
            die "More than one return instruction found" if @blocks != 1;
            $ret_block = $blocks[0];
            $fun->{llvm_return} = 'void';
        } else {
            my @blocks = $code =~ m/ReturnInst::Create\([^,]+, (\w+), (\w+)\);/g;
            die "No return instruction found" unless @blocks;
            die "More than one return instruction found" if @blocks != 2;
            ($ret_value, $ret_block) = @blocks;
            $fun->{llvm_return} = 'llvm::Value *';
        }
        # fix current function name in basic block creation
        $code =~ s[\w+${prefix}_\w+][function]g;
        # remove function creation and argument naming code
        $code =~ s[
            {
            \s+ Function .*? ;
        (?= \s+ BasicBlock \s* \* \s*)]
        [{]xms or die "Unable to remove function/parameter creation code";
        # these are not going to be tail calls
        $code =~ s[ (\w+) -> setTailCall\( true \) ;]
                  [// $1 is not a tail call]xgs;
        # fix reference to THX
        if ($Config{usethreads} eq 'define' && $fun->{prototype}{extra}{thx}) {
            $code =~ s/\b$args[0]\b/arg_thx/g;
            shift @args;
        }
        # fix references to global variables that stand in for local
        # function variables
        for my $v (qw(sp)) {
            $code =~ s [\b gvar_ptr_$v \b ][arg_$v]xg;
        }
        # use current insertion point as starting basic block
        $code =~ s[(BasicBlock \s* \* \s* \w+ \s* = ) .*? ;]
                  [$1 builder->GetInsertBlock();]x;
        # set the new insertion point and return the result value at
        # function end
        if ($fun->{prototype}{return} eq 'void') {
            $code =~ s[ReturnInst::Create.*?;]
                      [  builder->SetInsertPoint($ret_block);];
        } else {
            $code =~ s[ReturnInst::Create.*?;]
                      [builder->SetInsertPoint($ret_block);\n\n  return $ret_value;];
        }
        # rename module variable
        $code =~ s{\bmod\b}{module}g;

        $fun->{code} = $code;

        $fun->{llvm_proto} =
            join ', ', map "llvm::Value *$_", @args;

        # find used class members
        @matched_members{$code =~ m/$member_rx/g} = undef;
    }

    delete $matched_members{B};
    delete $matched_members{PAS};

    # fix variable declarations that are class members in the generated class
    $member_alt = join '|', sort { length($b) <=> length($a) }
                                 keys %matched_members;
    $initialization =~ s[
        ^ (\s*) (?:\w+ \s* (?: \* | \s)) \s* ($member_alt) (\s* (?: = | ;))
    ][$1$2$3]gmx;

    return {
        functions      => \%functions,
        initialization => $initialization,
        header         => $includes,
        members        => {
            map { $_ => $declarations{$_} } keys %matched_members,
        },
    };
}

sub _join_source {
    my ($functions) = @_;
    my $src = <<'EOT';
#include <EXTERN.h>
#include <perl.h>

SV **sp;
EOT

    for my $fun (@$functions) {
        my $thx = '';
        if ($fun->{extra}{thx}) {
            $thx = $fun->{proto} ? 'pTHX_' : 'pTHX';
        }

        $src .= sprintf <<'EOT',

%s
%s_%s(%s %s)
{
%s
}
EOT
            $fun->{return}, $prefix, $fun->{name}, $thx,
                $fun->{proto}, $fun->{code};
    }

    return $src;
}

sub _run_clang {
    my ($source) = @_;
    my ($fh, $tmp) = tempfile('c-XXXXX', UNLINK => 1);

    print $fh $source or die "Error writing C source to '$tmp'";

    my $rc;
    my ($stdout, $stderr) = capture {
        $rc = system("$clang $ccopts_no_dashg -S -emit-llvm -o - -x c $tmp");
    };
    die "Error running clang on source: '$stderr'" if $rc;

    return $stdout;
}

sub _run_llc {
    my ($ir) = @_;
    my ($fh, $tmp) = tempfile('bytecode-XXXXX', UNLINK => 1);

    print $fh $ir or die "Error writing IR to '$tmp'";

    my $rc;
    my ($stdout, $stderr) = capture {
        $rc = system($llc, '-march=cpp', '-o', '-', $tmp);
    };
    die "Error running llc on IR: '$stderr'" if $rc;

    return $stdout;
}

1;
