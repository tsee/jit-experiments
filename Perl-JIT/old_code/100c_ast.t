use strict;
use warnings;
BEGIN {
  push @INC, 't/lib', 'lib';
}
use Perl::JIT::Test;

run_ctest('100c_ast')
  or Test::More->import(skip_all => "C executable not found");


