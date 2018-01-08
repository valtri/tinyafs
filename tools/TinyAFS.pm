package TinyAFS;

use strict;
use warnings;

use Exporter qw(import);

our @EXPORT_OK = qw(quote listvol);

sub newvol() {
}

sub quote($$) {
	my ($quote, $unit) = @_;
	my $mult;

	if ($unit == 'K') { $mult = 1024; }
	elsif ($unit == 'M') { $mult = 1024**2; }
	elsif ($unit == 'G') { $mult = 1024**3; }
	else { $mult = 1; }

	return $mult * $quote;
}

sub listvol($) {
	my ($server, $part) = @_;
	my @list;
	my ($count, $max_count);
	if (not $server) { die "Missing server!"; }

	my @cmd = ('vos', 'listvol', '-server', $server);
	if ($part) {
		push @cmd, ['-partition', $part];
	}

	my $partition = undef;
	open(my $fh, "-|") || exec(@cmd) || die "Exec error: $!";
	while (my $line = <$fh>) {
		my $v;

		chomp $line;
		if ($line =~ /^Total number of volumes on server (.+) partition (.+): (\d+)\s*$/) {
			$server = $1;
			$partition = $2;
			$max_count = $3;
			$count = 0;
		} elsif ($line =~ /^Total volumes onLine (\d+)\s*;\s*Total volumes offLine (\d+)\s*;\s*Total busy (\d+)$/) {
		} elsif ($line =~ /^$/) {
		} elsif ($line =~ /^\*\*\*\* Volume (\d+) is busy \*\*\*\*$/) {
			$v->{id} = $1;
			$v->{status} = 'Busy';
		} else {
			my ($name, $id, $mode, $foo2, $foo3, $status) = split /\s+/, $line;
			$v->{name} = $name;
			$v->{id} = $id;
			$v->{mode} = $mode;
			$v->{status} = $status;
			$v->{partition} = $partition;
			$count = $count + 1;

			push @list, $v;
		}
	}
	close $fh;

	return @list;
}

1;
