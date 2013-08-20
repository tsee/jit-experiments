package Perl::JIT;
use strict;
use warnings;
our $VERSION = '0.01';

require XSLoader;
XSLoader::load("Perl::JIT", $VERSION);

use Exporter ();
our @ISA = qw(Exporter); # Oh, the pain.

our @EXPORT_OK; # filled-in by XS code
our %EXPORT_TAGS = ( 'all' => \@EXPORT_OK );

sub import {
  $^H{PJ_KEYWORD_PLUGIN_HINT()} = 1;
  __PACKAGE__->export_to_level(1, @_);
}

sub unimport {
  $^H{PJ_KEYWORD_PLUGIN_HINT()} = 0;
}

1;
__END__

=head1 NAME

Perl::JIT - Perl extension for blah blah blah

=head1 SYNOPSIS

  use Perl::JIT;

  BLAH QUICKLY!

=head1 DESCRIPTION

=head1 SEE ALSO

=head1 AUTHOR

Steffen Mueller, E<lt>smueller@cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2013 by Steffen Mueller

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.10.1 or,
at your option, any later version of Perl 5 you may have available.


=cut
