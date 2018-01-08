#! /usr/bin/perl

use strict;
use warnings;
use IO::Handle;
use TinyAFS;

my %volumes;
my @servers = @ARGV;
my $busy = 0;

STDERR->autoflush(1);

if ($#servers == -1) {
	@servers = (
		'elektra1.zcu.cz',
		'elektra2.zcu.cz',
		'elektra3.zcu.cz',
		'eurynome1.zcu.cz',
		'eurynome2.zcu.cz',
		'eurynome3.zcu.cz',
	);
}

my $server;

foreach $server (values @servers) {
	print STDERR "Server $server\n";
	my @server_volumes = TinyAFS::listvol($server);
	foreach my $vol (values @server_volumes) {
		if (not exists $vol->{name}) {
			$volumes{$vol->{id}} = 1;
			$busy = $busy + 1;
			print STDERR "Busy volume $vol->{id}\n";
			next;
		}
		if ($vol->{name} =~ /\.backup$/) {
			next;
		}
		$vol->{name} =~ s/\.readonly$//;
		$volumes{$vol->{name}} = 1;
	}
}

for my $vol (sort keys %volumes) {
	print "$vol\n";
}

if ($busy) {
	die "Busy volumes found!\n";
}
