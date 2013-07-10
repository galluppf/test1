#!/usr/bin/perl

##------------------------------------------------------------------------------
##
## sdping	    SDP ping program
##
## Copyright (C)    The University of Manchester - 2011
##
## Author           Steve Temple, APT Group, School of Computer Science
## Email            temples@cs.man.ac.uk
##
##------------------------------------------------------------------------------

use strict;
use warnings;

use IO::Socket::INET;
use POSIX;


my $version = '0.90';

my $sleep = 1.0;	# Repeat (secs)
my $socket;		# UDP connection socket
my $dp = 0;		# Port/CPU
my $da = 0;		# Chip number (x * 256 + y)
my $port = 17893;	# Target port on Spinnaker board

my $count = 1;
my $ascii = 1;


#------------------------------------------------------------------------------


# Send string in "$data" to Spinnaker CPU ($da, $dp). Uses UDP port 17893 by
# default

sub send_sdp
{
    my ($da, $dp, $data) = @_;
    my ($flags, $tag, $sa, $sp, $ipto, $len) = (0x87, 255, 0, 255, 1, length $data);

    my $hdr = pack 'C2 C4 v2', $ipto, 0, $flags, $tag, $dp, $sp, $da, $sa;
    my $cmd = pack 'V4', 0, 0, 0, 0;

    printf ">> F=%02x T=%02x DA=%04x DP=%02x SA=%04x SP=%02x ($len) \[$data\]\n",
      $flags, $tag, $da, $dp, $sa, $sp;

    my $rc = send ($socket, $hdr . $cmd . $data, 0);

    warn "!! send failed\n" unless defined $rc;
}


#------------------------------------------------------------------------------


# Pretty hex dumper for data in "$data"

sub hexdump
{
    my ($addr, $data, $format) = @_;

    my $ptr = 0;
    my $size = 16;
    my $text;

    while (1)
    {
	my $chunk = substr $data, $ptr, $size;
	my $len = length $chunk;
	last if $len == 0;

	$text .= sprintf "%08x ", $addr + $ptr;

	if ($format eq 'Byte')
	{
	    my @d = unpack 'C*', $chunk;
	    $text .= sprintf " %02x", $_ for @d;
	    $text .= '   ' for ($#d..$size-1);
	    $text .= ($_ < 32 || $_ > 127) ? '.' : chr $_ for @d;
	}
	elsif ($format eq 'Half')
	{
	    my @d = unpack 'v*', $chunk;
	    $text .= sprintf " %04x", $_ for @d;
	}
	elsif ($format eq 'Word')
	{
	    my @d = unpack 'V*', $chunk;
	    $text .= sprintf " %08x", $_ for @d;
	}

	$text .= "\n";
	$ptr += $len;
    }

    return $text;
}


#------------------------------------------------------------------------------


sub usage
{
    die "usage: sdping <hostname> <chipX> <chipY> <CPU> <port>\n";
}


sub process_args
{
    usage () unless $#ARGV == 4 && $ARGV[1] =~ /^\d+$/ && $ARGV[2] =~ /^\d+$/ &&
	$ARGV[3] =~ /^\d+$/ && $ARGV[4] =~ /^\d+$/ ;

    my $target = $ARGV[0];
    $da = $ARGV[1] * 256 + $ARGV[2];
    $dp = $ARGV[4] * 32 + $ARGV[3];

    $socket = new IO::Socket::INET (PeerAddr => "$target:$port",
				    Proto => 'udp',
				    Blocking => 0);

    die "Failed to connect to $target on port $port: $!\n" unless $socket;
}


#------------------------------------------------------------------------------


# Main loop which sends a ping SDP packet every "$sleep" seconds and looks
# for incoming reply packets. Both sent and received packets are printed.

sub main
{
    process_args ();

    my $string = "0123456789abcdef" . chr(0);

    while (1)
    {
	send_sdp ($da, $dp, $string);

	my $rd_mask = '';
	vec ($rd_mask, fileno ($socket), 1) = 1;

	my ($n, $time_left) = select ($rd_mask, undef, undef, $sleep);

	$time_left = 1 unless defined $time_left; # Just in case...

	if ($n > 0)
	{
	    my $addr = recv ($socket, my $data, 65536, O_NONBLOCK);

	    my (undef, $flags, $tag, $dp, $sp, $da, $sa, undef, undef, undef, undef, $d) =
		unpack 'v C4 v2 V4 a*', $data;

	    my $len = length $d;

	    if ($ascii)
	    {
		printf "[$count] F=%02x T=%02x DA=%04x DP=%02x SA=%04x SP=%02x ($len) \[$d\]\n",
		  $flags, $tag, $da, $dp, $sa, $sp;
	    }
	    else
	    {
	      printf "[$count] F=%02x T=%02x DA=%04x DP=%02x SA=%04x SP=%02x ($len)\n",
 	        $flags, $tag, $da, $dp, $sa, $sp;

	      print hexdump (0, $d, 'Byte') if $len > 0;
	    }
	    $count++;
	}

	select (undef, undef, undef, $time_left);
    }
}


main ();

#------------------------------------------------------------------------------
