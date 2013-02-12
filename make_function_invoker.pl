use strict;
use warnings;

my $n = shift(@ARGV) || 15;
print <<"HERE";
/* WARNING: Do not modify this file, it is generated!
 * Modify the generating script $0 instead! */

HERE
$\ = " \\\n";
print "#define PJ_TYPE_SWITCH(type, args, nargs, retval)";
print "  {";
print "  type *a = (type *)args;";
print "  switch (nargs) {";

foreach my $i (0..$n) {
  my $params = join(", ", ("type") x $i)||'void';
  my $args   = $i == 0 ? "" : join(", ", map "a[$_]", 0..($i-1));
  print "    case $i: {";
  print "      type (*f)($params) = (void *)fptr;";
  print "      *((type *)retval) = f($args);";
  print "      break; }";
}

print "    default:";
print "      abort();";
print "    }";

$\ = "\n";
print "  }";

