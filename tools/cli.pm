
##------------------------------------------------------------------------------
##
## cli.pm	    Perl library providing a simple CLI
##
## Copyright (C)    The University of Manchester - 2009, 2010
##
## Author           Steve Temple, APT Group, School of Computer Science
## Email            temples@cs.man.ac.uk
##
##------------------------------------------------------------------------------

package cli;


use strict;
use warnings;


#-----------------------------------------------------------------------------
# new - return a new CLI data structure
#
# arg1 - filehandle to read input from
# arg2 - prompt string
# arg3 - command list (a hash reference)
#
# returns - CLI data structure
#-----------------------------------------------------------------------------

sub new
{
    my $cli;

    $cli->{fh} = shift;
    $cli->{prompt} = shift;
    $cli->{cmd} = shift;

    $cli->{level} = 0;
    $cli->{quiet} = 0;
    $cli->{tty} = -t $cli->{fh};

    $cli->{arg_c} = 0;
    $cli->{arg_v} = [];
    $cli->{arg_n} = [];
    $cli->{arg_x} = [];

    bless $cli;

    return $cli;
}


#-----------------------------------------------------------------------------
# run - execute a CLI using supplied data structure
#
# arg1 - CLI data structure (eg made with "new")
#-----------------------------------------------------------------------------

sub run
{
    my $cli = shift;

    my $fh = $cli->{fh};
    my $tty = $cli->{tty};

    $| = ($tty) ? 1 : 0;

    while (1)
    {
	my $prompt = $cli->{prompt};
	my $cmds = $cli->{cmd};
	my $arg_v = $cli->{arg_v} = [];
	my $arg_n = $cli->{arg_n} = [];
	my $arg_x = $cli->{arg_x} = [];

	print $prompt if $tty;

	$_ = <$fh>;

	unless (defined $_)
	{
	    last unless $tty;
	    print "\n";
	    next;
	}

        print "$prompt$_" if !$tty && !$cli->{quiet};

	chomp;
	s/^\s*|\s*$//g;

	next if /^$/;
	next if /^\#/;

	@$arg_v = split;

	my $cmd = shift @$arg_v;
	my $ac = $cli->{arg_c} = @$arg_v;

	for (my $i = 0; $i < $ac; $i++)
	{
	    my $s = $arg_v->[$i];

	    $arg_n->[$i] = ($s =~ /^(-?\d+)$/) ? $1 + 0 : undef;
	    $arg_x->[$i] = ($s =~ /^([0-9A-Fa-f]+)$/) ? hex $1 : undef;
	}

	if (exists $cmds->{$cmd})
	{
	    if ($ac == 1 && $arg_v->[0] eq '?')
	    {
		my $h = $cmds->{$cmd}->[1];
		my $p = $cmds->{$cmd}->[2];
		print "usage:   $cmd $h\n";
		print "purpose: $p\n";
	    }
	    else
	    {
		my $proc = $cmds->{$cmd}->[0];
		my $rc = &$proc ($cli);
		last if $rc eq '1';
		print "error: $rc\n" if $rc;
	    }
	}
	else
	{
	    print "bad command \"$cmd\"\n";
	}
    }
}


#-----------------------------------------------------------------------------
# cmd - change command list and prompt
#
# arg1 - CLI data structure
# arg2 - new command list (hash reference)
# arg3 - new prompt
#
# returns - old command list and old prompt
#-----------------------------------------------------------------------------

sub cmd
{
    my ($cli, $new_cmd, $new_prompt) = @_;

    my $old_cmd = $cli->{cmd};
    my $old_prompt = $cli->{prompt};

    $cli->{cmd} = $new_cmd;
    $cli->{prompt} = $new_prompt;

    return ($old_cmd, $old_prompt);
}


#-----------------------------------------------------------------------------
# at - command to read CLI commands from a file
#
# arg1 - CLI data structure
#
# returns 0
#-----------------------------------------------------------------------------

sub at
{
    my $cli = shift;
    my $quiet = 0;

    return "filename expected" if $cli->{arg_c} < 1;
    return "\@ nested too deep" if $cli->{level} > 10;

    my $fn = $cli->{arg_v}->[0];

    $quiet = $cli->{arg_v}->[1] if $cli->{arg_c} > 1;

    open my $fh, '<', $fn or return "can't open \"$fn\"";

    my $old_fh = $cli->{fh};
    my $old_tty = $cli->{tty};
    my $old_prompt = $cli->{prompt};
    my $old_quiet = $cli->{quiet};

    $cli->{fh} = $fh;
    $cli->{tty} = -t $fh;
    $cli->{prompt} = '@' . $cli->{prompt};
    $cli->{quiet} = $quiet;
    $cli->{level}++;

    $cli->run;

    $cli->{level}--;
    $cli->{fh} = $old_fh;
    $cli->{tty} = $old_tty;
    $cli->{prompt} =~ s/^@//;
    $cli->{quiet} = $old_quiet;

    close $fh;

    return 0;
}


#-----------------------------------------------------------------------------
# quit - command to quit current CLI
#
# arg1 - CLI data structure
#
# returns 1
#-----------------------------------------------------------------------------

sub quit
{
    return 1;
}


#-----------------------------------------------------------------------------
# help - command to print help information on CLI commands
#
# arg1 - CLI data structure
#
# returns 0
#-----------------------------------------------------------------------------

sub help
{
    my $cli = shift;
    my $cmds = $cli->{cmd};

    printf " %-12s %-30s - %s\n", $_, $cmds->{$_}->[1], $cmds->{$_}->[2]
	for (sort keys %$cmds);

    return 0;
}


#-----------------------------------------------------------------------------
# query - command to print a list of CLI commands
#
# arg1 - CLI data structure
#
# returns 0
#-----------------------------------------------------------------------------

sub query
{
    my $cli = shift;
    my $cmds = $cli->{cmd};
    my $s = '';

    for (sort keys %$cmds)
    {
	if (length "$s $_" > 78)
	{
	    print "$s\n";
	    $s = '';
	}
	$s .= " $_";
    }
    print "$s\n" if $s ne '';

    return 0;
}


1;
