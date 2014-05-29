package Module::Build::Utils;
use 5.14.2;
use strict;
use warnings;
use autodie qw(open close);

use Exporter 'import';

our @EXPORT = qw(
    replace_if_changed
);

sub replace_if_changed {
    my ($path, $content) = @_;
    my $fh;

    if (-f $path) {
        local $/;
        open $fh, '<', $path;
        my $current = readline $fh;

        return if $content eq $current;
    }

    open $fh, '>', $path;
    print $fh $content;
    close $fh;
}

1;
