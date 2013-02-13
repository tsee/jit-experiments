package Perl::JIT;
use strict;
use warnings;
our $VERSION = '0.01';

require XSLoader;
XSLoader::load("Perl::JIT", $VERSION);

#sub import {
#  $^H{jit} = 1;
#}

#sub unimport {
#  $^H{jit} = 0;
#}

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
