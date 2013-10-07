package Module::Build::PerlJIT;
use 5.14.2;
use warnings;
use strict;

use Alien::LibJIT;
use Module::Build;
use parent 'Module::Build::WithXSpp';

__PACKAGE__->add_property('libjit');

use FindBin('$Bin');
use File::Spec;
use Config;

sub new {
    my ($class, @args) = @_;

    my $alien = Alien::LibJIT->new;
    my $self = $class->SUPER::new(
        libjit              => $alien,
        include_dirs        => ['.', $alien->include_dir],
        #extra_linker_flags  => [$alien->static_library, '-lpthread'],
        extra_linker_flags  => ['-lpthread'],
        extra_typemap_modules => {
          'ExtUtils::Typemaps::STL::String' => '0',
        },
        @args,
    );
    return $self;
}


sub ACTION_code {
    my ($self) = @_;
    
    my $rv = $self->SUPER::ACTION_code(@_);

    return $rv;
}

sub ACTION_realclean {
    my ($self) = @_;
    return $self->SUPER::ACTION_realclean(@_);
}

1;
