#!/usr/bin/env perl
use 5.14.2;
use warnings;

use JSON::XS ();
use Getopt::Long qw(GetOptions :config pass_through);
use File::Path qw(mkpath);
use Cwd qw(cwd);
use Capture::Tiny;
use List::Util qw(first);
use File::Copy qw(cp);

our @Dependencies = (
  'File::ShareDir',
  'B::Generate',
  'Moo',
  'ExtUtils::XSpp',
  'Module::Build::WithXSpp',
  'Capture::Tiny',
  'ExtUtils::CBuilder',
  'Test::Differences',
  'B::Utils',
  'ExtUtils::Typemaps::STL::String',
  'Alien::LibJIT',
);

# Verbosity defaults to 0, but may be -1==silent, or obviously higher
my $cfg_file;
our $Verbose;
GetOptions(
  'config|c=s' => \$cfg_file,
  'verbose|v:i' => sub {$Verbose = $_[1] // 1},
);
$Verbose //= 0;

# Depending on verbosity, we'll just capture or we'll actually show what's going on.
my $capture_sub        = $Verbose ? \&Capture::Tiny::tee        : \&Capture::Tiny::capture;
my $capture_stdout_sub = $Verbose ? \&Capture::Tiny::tee_stdout : \&Capture::Tiny::capture_stdout;
my $capture_merged_sub = $Verbose ? \&Capture::Tiny::tee_merged : \&Capture::Tiny::capture_merged;

my @common_options = (
  'perl|p=s',
);

my $cfg = MyConfig->new($cfg_file);
my $cmd = lc(shift(@ARGV) || "");

my %cmds = (
  build_perl    => \&cmd_build_perl, # build one or multiple perls
  install_deps  => \&cmd_install_deps, # install dependencies from CPAN
  install       => \&cmd_install, # install custom code
  'do'          => \&cmd_do, # run oneliner using one or multiple perls
  test          => \&cmd_test, # run one or multiple component tests
);

my $sub = $cmds{$cmd};
if ($sub) {
  $sub->($cfg);
}
else {
  die "Need command. Supported commands:\n" . join(" ", sort keys %cmds) . "\n";
}

print "Done.\n" if $Verbose >= 0;

exit(0);
#########################################################

sub cmd_test {
  my ($cfg) = @_;

  GetOptions(
    my $opt = {},
    @common_options,
    'clean|c',
  );

  my $custom_component = shift @ARGV;
  $custom_component = shift @ARGV if $custom_component
                                  && $custom_component eq '--';
  
  my @components;
  if (!defined $custom_component or $custom_component eq "all") {
    push @components, @{$cfg->{custom_order}};
  }
  else {
    push @components, $custom_component;
  }

  my @perls = grok_perl_list($opt);
  # Prep the perls
  foreach my $name (@perls) {
    my $perl = $cfg->get_perl($name);
    if (not $perl->can_load_dependencies()) {
      install_deps($cfg, $perl, $opt);
    }
  }

  my $cwd = cwd();
  foreach my $component (@components) {
    foreach my $name (@perls) {
      my $perl = $cfg->get_perl($name);
      my $perl_bin = $perl->executable;
      eval {
        chdir($cfg->{custom}{$component});
        my $build_pl = -f "dev_Build.PL" ? "dev_Build.PL" : "Build.PL";
        if ($opt->{clean}) {
          sys($perl_bin, qw(Build realclean));
          sys_fatal(qw(git clean -dxf));
        }
        if (not -d "blib") {
          sys_fatal($perl_bin, $build_pl);
          sys_fatal($perl_bin, qw(Build));
        }
        sys($perl_bin, qw(Build test));
        1
      } or do {
        chdir($cwd);
        die $@;
      };
      chdir($cwd);
    }
  }
}

sub cmd_build_perl {
  my ($cfg) = @_;

  GetOptions(
    my $opt = {},
    @common_options,
    'test'
  );

  my @perl_names = grok_perl_list($opt);

  foreach my $name (@perl_names) {
    my $perl = $cfg->get_perl($name);
    local $perl->{test} = 1 if $opt->{test};
    build_perl($cfg, $perl);
  }
}

sub cmd_install {
  my ($cfg) = @_;

  GetOptions(
    my $opt = {},
    @common_options,
    'test',
    'clean|c',
  );

  my @perls = grok_perl_list($opt);

  foreach my $name (@perls) {
    my $perl = $cfg->get_perl($name);
    local $perl->{test} = 1 if $opt->{test};
    if (not $perl->can_load_dependencies()) {
      install_deps($cfg, $perl, $opt);
    }
    install_custom($cfg, $perl, $opt);
  }
}

sub cmd_install_deps {
  my ($cfg) = @_;

  GetOptions(
    my $opt = {},
    @common_options,
    'test',
    'force',
  );

  my @perls = grok_perl_list($opt);

  foreach my $name (@perls) {
    my $spec = $cfg->get_perl($name);
    local $spec->{test} = 1 if $opt->{test};
    install_deps($cfg, $spec, $opt);
  }
}

sub cmd_do {
  my ($cfg) = @_;

  GetOptions(
    my $opt = {},
    @common_options,
  );

  my @args = @ARGV;
  shift @args if @args and $args[0] eq '--';

  my @perls = grok_perl_list($opt);

  foreach my $name (@perls) {
    my $spec = $cfg->get_perl($name);
    my $perl_cmd = $spec->executable;
    sys_fatal($perl_cmd, @args);
  }
}


sub install_custom {
  my ($cfg, $spec, $opt) = @_;

  my $custom_order = $cfg->{custom_order};
  my $custom = $cfg->{custom};

  my $perl = $spec->executable;

  foreach my $custom_name (@{$custom_order}) {
    my $path = $custom->{$custom_name};
    my $cwd = cwd();
    eval {
      chdir($path) or die "Failed to chdir to '$path'";
      if ($opt->{clean} && -f "Build") {
        sys($perl, qw(Build realclean)); # non-fatal
      }

      if (not -f "dev_Build.PL") {
        sys_fatal("dzil", "build");
        opendir my $dh, "." or die $!;
        my @dist_dirs = map {
            if (-f $_ and /^(.*?)-v?\d+(?:\.\d+)?\.tar.gz$/) {
              $1
            }
            else {
              ()
            }
          }
          readdir($dh);
        die "Cannot find dist dir after dzil build" if not @dist_dirs;
        print "Found dzil-generated dist-dir at '$dist_dirs[0]'\n" if $Verbose >= 1;
        print "Copying Build.PL as dev_Build.PL\n" if $Verbose >= 0;
        cp(File::Spec->catfile($dist_dirs[0], "Build.PL"), "dev_Build.PL");
      }

      sys_fatal($perl, "dev_Build.PL");
      sys_fatal($perl, "Build", "test") if $opt->{test};
      sys_fatal($perl, "Build", "install");
      1
    } or do {
      chdir($cwd);
      die $@;
    };
    chdir($cwd) or die "Failed to chdir to '$cwd'";
  }
}

sub install_deps {
  my ($cfg, $perl, $opt) = @_;
  build_perl($cfg, $perl) if not $perl->is_built;

  my $cpan_cmd = $perl->executable("cpan");
  die "Strange, can't seem to run '$cpan_cmd'" if not -x $cpan_cmd;

  my @deps = grep {
                $opt->{force}
                or not($perl->can_load_dependency($_))
             } @Dependencies;
  if (@deps) {
    print "Installing the following dependencies: @deps\n"
      if $Verbose >= 0;
    sys_fatal($cpan_cmd, @deps);
  }
  else {
    print "All dependencies already available. Use --force to override.\n"
      if $Verbose >= 0;
  }
}


sub build_perl {
  my ($cfg, $perl) = @_;

  my $name = $perl->{name};
  print "Building perl '$name'...\n" if $Verbose >= 0;
  my $out_dir = $perl->{path};
  mkpath($out_dir);

  my $cwd = cwd();
  chdir($cfg->{perl_repo}) or die "Failed to chdir to '$cfg->{perl_repo}'";
  eval {
    sys_fatal(qw(git clean -dxf));
    sys_fatal(qw(git reset --hard HEAD));
    sys_fatal(qw(git checkout ), $perl->{tag});
    my $configure_cmd = $perl->{configure};
    $configure_cmd =~ s/\$install_path\b/$out_dir/g;
    sys_fatal($configure_cmd);
    my @make_opts;
    push @make_opts, "-j$cfg->{j}" if $cfg->{j};
    if ($perl->{test}) {
      local $ENV{TEST_JOBS} = $cfg->{j};
      sys_fatal(qw(make), @make_opts, ($cfg->{j} ? "test_harness" : "test"));
    }
    sys_fatal(qw(make), @make_opts, "install");

    my $bindir = File::Spec->catdir($out_dir, "bin");
    chdir($bindir) or die "Failed to chdir to '$bindir'";
    foreach (glob("*5.*")) {
      my $n = $_;
      s/5\.\d+\.\d+// or next;
      local $Verbose = $Verbose - 1;
      sys_fatal(qw(ln -s), $n, $_);
    }

    chdir($cwd) or die "Failed to chdir back to '$cwd'";
    1
  } or do {
    chdir($cwd);
    die $@;
  }
}

#####################################
# q&d helper functions

sub grok_perl_list {
  my ($opt) = @_;

  my $perl = $opt->{perl} // "all";
  die "Invalid Perl"
    if $perl ne "all"
    and not $cfg->get_perl($perl);

  my @perls = ($perl eq 'all'
               ? keys %{$cfg->{perls}}
               : $perl);

  return @perls;
}

sub sys_fatal {
  print "Running command '@_'...\n" if $Verbose >= 0;
  my @args = @_;
  my $exit;
  my $str = $capture_merged_sub->(sub {
    $exit = system(@args);
  });
  if ($Verbose >= 0) {
    $exit and die "Failed to run command \"@_\": $?. Output was:\n$str";
  }
  else {
    $exit and die "Failed to run command \"@_\": $?";
  }
  return $exit;
}

sub sys {
  # using global $Verbose, sue me
  print "Running command '@_'...\n" if $Verbose >= 0;
  my @args = @_;
  my $exit;
  my $str = $capture_merged_sub->(sub {
    $exit = system(@args);
  });
  return $exit;
}


package MyConfig {
  sub new {
    my $class = shift;
    my $file = shift;
    if (not defined $file) {
      $file =   -f $ENV{HOME} . '/.jitperls.cfg' ? $ENV{HOME} . '/.jitperls.cfg'
           : -f 'jitperls.cfg'               ? 'jitperls.cfg'
           : -f 'author_tools/jitperls.cfg'  ? 'author_tools/jitperls.cfg'
           : undef;
      die "Need config file or config file standard location" if not defined $file;
    }

    my $j = JSON::XS->new;
    $j->relaxed(1);
    my $cfg = $j->decode(do {local $/; open my $fh, "<", $file or die $!; <$fh>});

    # expand ~ so we don't rely on shells all over
    $cfg->{$_} =~ s/^~/$ENV{HOME}/ for qw(perl_repo perl_out_dir);
    foreach my $c (keys %{$cfg->{custom}}) {
      $cfg->{custom}{$c} =~ s/^~/$ENV{HOME}/;
    }

    my $perls = $cfg->{perls};
    foreach my $name (keys %$perls) {
      $perls->{$name} = MyPerl->new($cfg, $name, $perls->{$name});
    }

    return bless($cfg => $class);
  }

  sub get_perl {
    my ($self, $name) = @_;
    return $self->{perls}{$name};
  }
}; # end package MyConfig

package MyPerl {
  sub new {
    my ($class, $cfg, $name, $spec) = @_;
    $spec->{name} = $name;
    $spec->{path} = File::Spec->catdir($cfg->{perl_out_dir}, $name);
    return bless($spec => $class);
  }

  sub executable {
    my $self = shift;
    my $which = shift // "perl";
    return File::Spec->catfile($self->{path}, "bin", $which);
  }

  sub is_built {
    my $self = shift;
    return -x $self->executable;
  }

  sub can_load_dependency {
    my ($self, $dep) = @_;

    return() if not $self->is_built;

    local $Verbose = $Verbose - 1;
    my $exit = ::sys($self->executable, "-M$dep", "-e1");
    return $exit ? 0 : 1;
  }

  sub can_load_dependencies {
    my ($self) = @_;

    return() if not $self->is_built;

    local $Verbose = $Verbose - 1;
    my $exit = ::sys($self->executable, (map "-M$_", @Dependencies), "-e1");
    return $exit ? 0 : 1;
  }
}; # end package MyPerl
