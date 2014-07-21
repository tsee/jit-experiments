package Module::Build::PerlJIT;
use 5.14.2;
use warnings;
use strict;
use autodie qw(open);
use lib '../Algorithm-Burs/lib';

use Config;
use Module::Build;
use parent 'Module::Build::WithXSpp';

use Module::Build::LLVM qw(create_function_declaration create_emitter_class);
use Module::Build::Codegen qw(create_codegen_class);

sub new {
    my ($class, %args) = @_;

    my ($cppflags, $libs) = $class->_llvm_config;
    my $self = $class->SUPER::new(
        include_dirs => ['.'],
        extra_compiler_flags => [@{delete $args{extra_compiler_flags} || []}, $cppflags],
        extra_linker_flags  => [@{delete $args{extra_linker_flags} || []}, $libs],
        extra_xspp_flags    => [qw(--no-exceptions)],
        extra_typemap_modules => {
          'ExtUtils::Typemaps::STL::String' => '0',
        },
        %args,
    );
    return $self;
}

sub _llvm_config {
    my ($class) = @_;

    my $llvmc = $ENV{LLVM_CONFIG} || 'llvm-config';

    my $vers = `$llvmc --version`; chomp $vers;
    die "Error running '$llvmc --version'" if $?;

    unless ($vers =~ /^(\d+\.\d+)/ and $1 >= 3.1) {
        print "Wrong LLVM version '$vers': needed 3.1 or better\n";
        exit 1;
    }

    my $cppflags = `$llvmc --cxxflags`; chomp $cppflags;
    die "Error running '$llvmc --cxxflags'" if $?;
    $cppflags =~ s{(?:^|\s)-W[\w\-]+(?=\s|$)}{ }g;

    if ($Config::Config{ccname} =~ /^(?:gcc|clang)$/) {
      # LLVM likely comes with "-W -fpedantic". Let's silence a few
      # noisy warnings LLVM vs. Perl compile flag and code interactions.
      # For example, Perl may, on this platform, choose to rely on
      # variadic macros because it knew that they're supported. No point
      # being fussy about it.
      $cppflags .= ' -Wno-variadic-macros -Wno-long-long';
    }

    my $libs = `$llvmc --ldflags --libs`;
    die "Error running '$llvmc --ldflags --libs'" if $?;
    $libs =~ s/\n/ /gs;

    return ($cppflags, $libs);
}

sub ACTION_code {
    my ($self) = shift;

    $self->depends_on('perlapi');
    $self->depends_on('codegen');
    $self->SUPER::ACTION_code(@_);
}

sub ACTION_perlapi {
    my ($self) = shift;

    $self->log_info("Running code generation for Perl API wrappers.\n");

    my $source = do {
        local $/;
        open my $fh, '<', 'src/perlapi.txt';
        readline $fh;
    };
    my @prototypes;

    $source =~ s/^ \s+ //x;
    while ($source) {
        $source =~ s[
            ^ (.*?) \s+ \( ([^)]*) \) \s+ {
            (.*?)
            ^ } \s+
        ][]xms or die "Unable to parse '$source'";
        my $fun = create_function_declaration($1);
        $fun->{extra} = { map { $_ => 1 } split /\s+/, $2 };
        $fun->{code} = $3;
        push @prototypes, $fun;
    }

    $self->add_to_cleanup('src/pj_perlapibase.h', 'src/pj_perlapibase.cpp');

    create_emitter_class(
        \@prototypes, ['PerlJIT'], 'PerlAPIBase',
        'src/pj_perlapibase.h', 'src/pj_perlapibase.cpp',
    );
}

sub ACTION_codegen {
    my ($self) = @_;

    $self->log_info("Running code generation for compiler.\n");

    $self->add_to_cleanup(
        'src/pj_codegen.h', 'src/pj_codegen.cpp', 'src/pj_rules.cpp',
        'src/pj_ast_map_ops-gen.inc',
    );
    create_codegen_class(
        'Codegen', 'Emitter',
        'src/pj_codegen.h', 'src/pj_codegen.cpp', 'src/pj_rules.cpp',
        'src/pj_ast_map_ops-gen.inc',
    );
}

1;
