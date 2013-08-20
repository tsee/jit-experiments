#!/usr/bin/env perl

use t::lib::Perl::JIT::Test;
use Perl::JIT qw(:all);

# Test a few constants to act as a canary if the
# constant-wrapping system fails

ok(defined(PJ_KEYWORD_PLUGIN_HINT));
ok(defined(pj_double_type));
ok(defined(pj_listop_LAST));
ok(defined(pj_unop_log));
my $jit = new_ok("Perl::JIT::Emit");

done_testing();
