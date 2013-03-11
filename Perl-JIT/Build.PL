use 5.14.2;
use warnings;
use strict;

use Config qw(%Config);
use FindBin qw($Bin);
use File::Spec;
use lib File::Spec->catdir($Bin, 'inc');
use Getopt::Long qw(GetOptions);
my $DEBUG = $ENV{DEBUG};
my $CTESTS = $ENV{CTESTS};
GetOptions(
  'debug|DEBUG+' => \$DEBUG,
  'ctest|ctests' => \$CTESTS,
);

use Module::Build::PerlJIT;

my @extra_compiler_flags;
if ($DEBUG) {
  push @extra_compiler_flags, qw(-O0 -ggdb3 -Wall -Wextra -Wno-unused-function);
  if ($DEBUG > 1) {
    push @extra_compiler_flags, qw(-pedantic);
  }
  open my $fh, ">", "DEBUGGING" or die $!;
  close $fh or die $!;
}
else {
  push @extra_compiler_flags, qw(-DNDEBUG);
}
if ($CTESTS) {
  open my $fh, ">", "CTESTS" or die $!;
  close $fh or die $!;
}

Module::Build::PerlJIT->new(
    ##{ $plugin->get_default('dist_name') ##}
    ##{ $plugin->get_default('all_from') ##}
    ##{ $plugin->get_default('module_name') ##}
    ##{ $plugin->get_default('main_module') ##}
    ##{ $plugin->get_default('license') ##}
    ##{ $plugin->get_default('dist_author') ##}
    ##{ $plugin->get_prereqs ##}
    ##{ $plugin->get_default('create_readme') ##}
    dynamic_config => 0,
    needs_compiler => 1,
    c_source => ['src'],
    xs_files => {'JIT.xs' => 'lib/Perl/JIT.xs'},
    include_dirs => [ '.', $Module::Build::PerlJIT::LIBJIT_INCLUDE ],
    extra_linker_flags => [$Module::Build::PerlJIT::LIBJIT_RESULT, '-lpthread'],
    extra_compiler_flags => \@extra_compiler_flags,
)->create_build_script;

exit(0);
