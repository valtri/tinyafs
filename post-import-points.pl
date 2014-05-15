#! /usr/bin/perl -w
use strict;
use DBI;

my $dbhost = 'localhost';
my $dbname = 'afs';
my $dbuser = 'afs';
my $pass = '';
my $dbcs;

my ($dbh, $sti, $stu, $stufull, $sts);
my ($mount, %mounts, $volume, $dir);
my $count = -1;
my $i = 0;

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
$sti = $dbh->prepare("INSERT INTO mountpoints (pointvolume, pointdir, volume) VALUES (?, ?, ?)");
$stu = $dbh->prepare("UPDATE mountpoints SET dir=? WHERE volume=?");
$stufull = $dbh->prepare("UPDATE mountpoints SET dir=? WHERE volume=? AND pointvolume=? AND pointdir=?");
$sts = $dbh->prepare("SELECT m1.volume volume, m1.pointvolume pointvolume, m1.pointdir pointdir, CONCAT(m2.dir, '/', m1.pointdir) dir FROM mountpoints m1, mountpoints m2 WHERE m1.pointvolume=m2.volume AND m2.dir IS NOT NULL AND m1.dir IS NULL");

$dbh->do("DELETE FROM mountpoints WHERE volume='root.cell'");
$sti->execute(undef, 'afs/zcu.cz', 'root.cell');
$stu->execute('/afs/zcu.cz', 'root.cell');


while ($count != 0 and $i < 10) {
	$count = 0;
	%mounts = ();

	$sts->execute();
	$dbh->begin_work;
	while (($mount = $sts->fetchrow_hashref())) {
#print STDOUT "$mount->{dir}, $mount->{volume}\n";
		$stufull->execute($mount->{dir}, $mount->{volume}, $mount->{pointvolume}, $mount->{pointdir});
		$count++;
	}
	$dbh->commit;

	$i += 1;
	print STDERR "$count\n";
}

$dbh->do("DELETE FROM volumes");
# dirty hack from http://www.xaprb.com/blog/2006/12/07/how-to-select-the-firstleastmax-row-per-group-in-sql/,
# ale bacha, vyžaduje ostrou nerovnost v každém řádku ==> porovnáván i přímo řetězec dir
$dbh->do("INSERT INTO volumes (volume, dir)
	SELECT volume, dir
		FROM mountpoints m
		WHERE (
			SELECT COUNT(*) FROM mountpoints m1
			WHERE m1.volume = m.volume AND LENGTH(m1.dir) <= LENGTH(m.dir) AND m1.dir > m.dir
		) <= 1");

$dbh->disconnect;
