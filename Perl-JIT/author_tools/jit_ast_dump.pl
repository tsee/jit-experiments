use 5.14.0;
use warnings;
use blib;
use Perl::JIT;
use Perl::JIT::Emit;
use Getopt::Long qw(GetOptions);

my $code;
GetOptions(
  'e=s' => \$code,
);
defined $code or die;

my $sub;
eval "no strict; \$sub = sub {$code}";
die $@ if $@;

my @asts = Perl::JIT::find_jit_candidates($sub);
for (@asts) {
  print "----------------------------------------------\n";
  $_->dump()
}
print "----------------------------------------------\n";

