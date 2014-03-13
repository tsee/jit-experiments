package Module::Build::PerlJIT;
use 5.14.2;
use warnings;
use strict;

use Module::Build;
use parent 'Module::Build::WithXSpp';

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

    unless ($vers >= 3.1) {
        print "Wrong LLVM version $vers: needed 3.1\n";
        exit 1;
    }

    my $cppflags = `$llvmc --cxxflags`; chomp $cppflags;
    die "Error running '$llvmc --cxxflags'" if $?;
    $cppflags =~ s{(?:^|\s)-W[\w\-]+(?=\s|$)}{ }g;

    my $libs = `$llvmc --ldflags --libs`;
    die "Error running '$llvmc --ldflags --libs'" if $?;
    $libs =~ s/\n/ /gs;

    return ($cppflags, $libs);
}

1;
