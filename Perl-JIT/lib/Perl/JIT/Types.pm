package Perl::JIT::Types;

use strict;
use warnings;

use Perl::JIT;
use attributes;

our $OLD_IMPORT = \&attributes::import;

SCOPE: {
  no warnings 'redefine';
  *attributes::import = \&replace_attributes_import;
}

1;
