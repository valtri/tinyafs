#! /usr/bin/perl -w
#
# :%s/\([^,]*\),\([^,]*\),\([^,]*\),\(.*\)/INSERT INTO statistics (login, volumes_read, volumes_write, volumes_admin, volumes_changed) VALUES ('\1', \2, \3, \4, sysdate);
#
# SELECT s.login, volumes_admin, wfstatus FROM user_statistics s, idm_users u WHERE s.login=u.login(+) AND volumes_admin > 10 ORDER BY volumes_admin;
# SELECT s.login, volumes_write, wfstatus FROM user_statistics s, idm_users u WHERE s.login=u.login(+) AND volumes_write > 10 ORDER BY volumes_write;
# SELECT s.login, volumes_own, wfstatus FROM user_statistics s, idm_users u WHERE s.login(+)=u.login AND volumes_own > 5 ORDER BY volumes_own;
#
sdate());/
use strict;
use DBI;

my $dbhost = 'localhost';
my $dbname = 'afs';
my $dbuser = 'afs';
my $pass = '';
my ($dbcs, $dbh);
my ($sth, $acl, $login, $volume);
my %counters;

if (open FH, '<', 'pass') {
	while (<FH>) {
		chomp;
		if (/(.*)=(.*)/) {
			if ($1 eq 'user') { $dbuser = $2; }
			if ($1 eq 'password') { $pass = $2; }
			if ($1 eq 'host') { $dbhost = $2; }
			if ($1 eq 'database') { $dbname = $2; }
		}
	}
	close FH;
}
$dbcs = "dbi:mysql:database=$dbname;host=$dbhost";

$dbh = DBI->connect($dbcs, $dbuser, $pass, {PrintError => 0, RaiseError => 0}) or die "Can't connect: $DBI::errstr\n";
$dbh->{RaiseError} = 1;

$sth = $dbh->prepare("SELECT login, volume, rights FROM rights");
$sth->execute;
while (($acl = $sth->fetchrow_hashref())) {
	$login = $acl->{login};
	$volume = $acl->{volume};

	if ($acl->{rights} =~ /.*r.*/) {
		$counters{$login}{r}{$volume} = 1;
#printf STDERR "added R: $acl->{login}, $acl->{volume}, $acl->{rights}\n";
	}
	if ($acl->{rights} =~ /.*w.*/) {
		$counters{$login}{w}{$volume} = 1;
	}
	if ($acl->{rights} =~ /.*a.*/) {
		$counters{$login}{a}{$volume} = 1;
	}
#printf STDERR "$acl->{login}, $acl->{volume}, $acl->{rights}\n";
}
$sth->finish;

$dbh->disconnect();

print "LOGIN,READ,WRITE,ADMIN\n";
for $login (keys %counters) {
	print "$login,";

	if (exists $counters{$login}{r}) {
		my @volumes = keys %{$counters{$login}{r}};
		print $#volumes + 1;
	}
	else { print '0'; }
	print ',';

	if (exists $counters{$login}{w}) {
		my @volumes = keys %{$counters{$login}{w}};
		print $#volumes + 1;
	}
	else { print '0'; }
	print ',';

	if (exists $counters{$login}{a}) {
		my @volumes = keys %{$counters{$login}{a}};
		print $#volumes + 1;
	}
	else { print '0'; }
	print "\n";
}
