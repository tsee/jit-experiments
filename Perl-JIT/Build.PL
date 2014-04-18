use 5.14.2;
use warnings;
use strict;

use Config qw(%Config);
use FindBin qw($Bin);
use File::Spec;
use lib File::Spec->catdir($Bin, 'inc');
use Getopt::Long qw(GetOptions);
my $DEBUG = $ENV{DEBUG};
my $DEBUG_SYMBOLS = $ENV{DEBUG_SYMBOLS};
my $ASSERTS = $ENV{ASSERTS};
GetOptions(
  'debug|DEBUG+' => \$DEBUG,
  'g' => \$DEBUG_SYMBOLS,
  'a' => \$ASSERTS,
);

use Module::Build::PerlJIT;

# Likely among other things, this avoids the use of braced groups, a GCC extension.
# These would otherwise cause a slurry of warnings.
my @extra_compiler_flags = qw(-DPERL_GCC_PEDANTIC);
if ($DEBUG) { # superset of $DEBUG_SYMBOLS
  push @extra_compiler_flags, qw(-O0 -ggdb3 -Wall -Wextra -DDEBUG_OUTPUT);
  if ($DEBUG > 1) {
    push @extra_compiler_flags, qw(-pedantic);
  }
  else {
    push @extra_compiler_flags, qw(-Wno-unused-function -Wno-unused-parameter -Wno-unused-variable);
  }
}
elsif ($DEBUG_SYMBOLS) { # subset of $DEBUG
  push @extra_compiler_flags, qw(-O0 -ggdb3 -Wall -Wextra);
  push @extra_compiler_flags, qw(-Wno-unused-function -Wno-unused-parameter -Wno-unused-variable);
}

if (!$ASSERTS and !$DEBUG) {
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
