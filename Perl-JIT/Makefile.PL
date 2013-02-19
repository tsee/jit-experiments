use 5.010001;
use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

my $extra_flags = '';
if ((grep {/--debug/} @ARGV) or $ENV{DEBUG}) {
  $extra_flags .= '-O0 -ggdb3';
}
else {
  $extra_flags .= '-DNDEBUG';
}

WriteMakefile(
    NAME              => 'Perl::JIT',
    VERSION_FROM      => 'lib/Perl/JIT.pm', # finds $VERSION
    PREREQ_PM         => {}, # e.g., Module::Name => 1.1
    ($] >= 5.005 ?     ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM  => 'lib/Perl/JIT.pm', # retrieve abstract from module
       AUTHOR         => 'Steffen Mueller <smueller@cpan.org>') : ()),
    LIBS              => ['-ljit'], # e.g., '-lm'
    DEFINE            => $extra_flags, # e.g., '-DHAVE_SOMETHING'
    INC               => '-I.', # e.g., '-I. -I/usr/include/other'
    OBJECT            => '$(O_FILES)', # link all the C files too
);
