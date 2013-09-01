use 5.14.2;
use warnings;
use strict;

use Config qw(%Config);
use FindBin qw($Bin);
use File::Spec;
use lib File::Spec->catdir($Bin, 'inc');
use Getopt::Long qw(GetOptions);
my $DEBUG = $ENV{DEBUG};
GetOptions(
  'debug|DEBUG+' => \$DEBUG,
);

use Module::Build::PerlJIT;

my @extra_compiler_flags;
if ($DEBUG) {
  push @extra_compiler_flags, qw(-O0 -ggdb3 -Wall -Wextra);
  if ($DEBUG > 1) {
    push @extra_compiler_flags, qw(-pedantic);
  }
  else {
    push @extra_compiler_flags, qw(-Wno-unused-function -Wno-unused-parameter -Wno-unused-variable);
  }

  open my $fh, ">", "DEBUGGING" or die $!;
  close $fh or die $!;
}
else {
  push @extra_compiler_flags, qw(-DNDEBUG);
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
    extra_compiler_flags => \@extra_compiler_flags,
)->create_build_script;

exit(0);
