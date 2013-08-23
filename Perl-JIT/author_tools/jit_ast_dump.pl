use 5.14.0;
use warnings;
use blib;
use Perl::JIT;
use Perl::JIT::Emit;
use Getopt::Long qw(GetOptions);
use B::Concise;

my $code;
my $concise;
GetOptions(
  'e=s' => \$code,
  'concise|c:s' => \$concise,
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

if (defined $concise) {
  open(STDERR, ">&STDOUT")     or die "Can't dup STDOUT: $!";
  $concise = '' if $concise eq '1';
  B::Concise::compile($concise,'',$sub)->();
}
