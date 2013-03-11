package Module::Build::PerlJIT;
use 5.14.2;
use warnings;
use strict;

use Module::Build;
use parent 'Module::Build';

use FindBin('$Bin');
use File::Spec;
use Config;

our $LIBJIT_HOME = 'libjit';
our $LIBJIT_M4 = 'm4';
our $LIBJIT_INCLUDE = File::Spec->catfile($LIBJIT_HOME, 'include');
our $LIBJIT_RESULT = File::Spec->catfile($LIBJIT_HOME, 'jit', '.libs', 'libjit'.$Config::Config{lib_ext});
my $ctest_dir = 'ctest';

sub ACTION_code {
    my ($self) = @_;
    
    $self->depends_on('libjit');
    
    my $rv = $self->SUPER::ACTION_code(@_);

    $self->build_ctests() if -f 'CTESTS';

    return $rv;
}

sub build_ctests {
    my ($self) = @_;
    
    my @test_cfiles = glob("$ctest_dir/*.c");
    my @test_exefiles;
    foreach my $file (@test_cfiles) {
        (my $exefile = $file) =~ s/\.c$/$Config::Config{exe_ext}/;
        push @test_exefiles, $exefile;
    }
    print "--ctests enabled: Will build C tests:\n  ",
        join("\n  ", @test_exefiles), "\n";

    #my @extra_objs = glob("src/*".$Config::Config{obj_ext});
    my @extra_objs = map File::Spec->catfile('src', "$_$Config::Config{obj_ext}"),
        qw(
            pj_ast_jit
            pj_ast_terms
            pj_ast_walkers
        );

    my $cb = $self->cbuilder;
    for (0..$#test_cfiles) {
        my $srcfile = $test_cfiles[$_];
        my $exefile = $test_exefiles[$_];
        my $obj = $cb->compile(
            extra_compiler_flags => [qw(-Isrc), @{$self->extra_compiler_flags}],
            source => $srcfile,
        );
        $cb->link_executable(
            extra_linker_flags => [@{$self->extra_linker_flags}, qw(-lm)],
            objects => [$obj, @extra_objs]
        );
    }
}

sub ACTION_clean {
    my ($self) = @_;
    my @test_cfiles = glob("$ctest_dir/*.c");
    my @test_exefiles;
    my @test_objfiles;
    foreach my $file (@test_cfiles) {
        (my $exefile = $file) =~ s/\.c$/$Config::Config{exe_ext}/;
        push @test_exefiles, $exefile;
        $exefile =~ s/\Q$Config::Config{exe_ext}$//;
        $exefile .= $Config::Config{obj_ext};
        push @test_objfiles, $exefile;
    }

    unlink $_ for (@test_exefiles, @test_objfiles);
    return $self->SUPER::ACTION_clean(@_);
}

sub ACTION_realclean {
    my ($self) = @_;
    unlink("DEBUGGING");
    unlink("CTESTS");
    return $self->SUPER::ACTION_realclean(@_);
}

sub ACTION_depcheck {
    my ($self) = @_;
    foreach my $cmd (qw/autoreconf libtool flex bison/) {
        $self->log_info("Checking if '$cmd' is available\n");
        system("$cmd --help > /dev/null 2>&1")
            and die "You need to make sure you have a recent '$cmd' installed and " .
                'that it can be found in your PATH';
    }
    return 1;
}

sub ACTION_libjit {
    my ($self) = @_;

    if(-f $LIBJIT_RESULT) {
        $self->log_info("libjit already built\n");
        return 1;
    }

    $self->depends_on('depcheck');
    
    my $orig = Cwd::cwd();
    
    $self->log_info("Changing directories to build libjit\n");
    chdir($LIBJIT_HOME) or die "Failed to cd into '$LIBJIT_HOME'";
    
    $self->log_info("Creating '$LIBJIT_M4' directory for autoreconf\n");
    mkdir($LIBJIT_M4) or die "Failed to mkdir '$LIBJIT_M4'";

    $self->log_info("Running autoreconf\n");
    system('autoreconf', '-i', '-f')
        and die "Failed to run autoreconf";
    
    $self->log_info("Running ./configure\n");
    #system('./configure', '-enable-shared=false')
    $ENV{CFLAGS} .= " -fPIC";
    system('./configure')
        and die "Failed to configure libjit!";
    
    $self->log_info("Running make\n");
    system('make') and die "Failed to build libjit!";
    
    $self->log_info("Returning to our original directory\n");

    chdir($orig);

    if(-f $LIBJIT_RESULT) {
        $self->log_info("Built libjit successfully\n");
        return 1;
    }
    else {
        die "We built libjit, but the lib isn't where I wanted it: $LIBJIT_RESULT";
    }
}

1;
