#! /usr/bin/perl -w
#
# Showing rights in form of commands clearing the rights.
#
# It tries to do some graph optimization: skips already cleared subtrees and
# process all their siblings instead.
#
use strict;
use DBI;

my $dbhost = 'localhost';
my $dbname = 'afs';
my $dbuser = 'afs';
my $pass = '';
my $default_servicedir = '$servicedir';
my $dbcs;
my $servicedir;

my ($dbh, $sthseed, $sthsubtreelist, $sthdir, $sthvolumes);
my ($dirrec, $subrec, $volrec, $voldir);
my ($path, $skip, $new_vortex);
my $login;
my %cajk = ();
my %cajk_individual = ();
my %volumes = ();

if ($#ARGV + 1 <= 0) {
       print "Usage: $0 LOGIN [SERVICE_DIRECTORY]\n";
       exit 1;
}
$login = $ARGV[0];
if ($#ARGV + 1 >= 1) {
	$servicedir = $ARGV[1];
}

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
$dbh->do('SET NAMES \'UTF8\'');
if ($login) {
	$sthseed = $dbh->prepare("SELECT * FROM rights WHERE login = ? ORDER BY volume, dir");
} else {
	# zatim nejde (v kódu napočítáme s víc loginama)
	$sthseed = $dbh->prepare("SELECT * FROM rights WHERE login LIKE '0%' OR login LIKE '1%' OR login LIKE '2%' OR login LIKE '3%' OR login LIKE '4%' OR login LIKE '5%' OR login LIKE '6%' OR login LIKE '7%' OR login LIKE '8%' OR login LIKE '9%' ORDER BY volume, dir");
}
$sthsubtreelist = $dbh->prepare("SELECT DISTINCT dir FROM rights WHERE volume = ? AND dir LIKE ?");
$sthdir = $dbh->prepare('SELECT * FROM rights WHERE volume = ? AND dir = ? AND login = ? AND rights = ?');
$sthvolumes = $dbh->prepare('SELECT * FROM volumes');

# seed
if ($login) {
	$sthseed->execute($login);
} else {
	$sthseed->execute();
}
while (($dirrec = $sthseed->fetchrow_hashref())) {
#print STDERR "seed ($dirrec->{volume}): '$dirrec->{dir}', $dirrec->{rights}\n";

	# skip if in known subtrees
	$path = '';
	$skip = 0;
	foreach my $subdir (split(/\//, $dirrec->{dir})) {
		next if (!$subdir);
#print STDERR "path ($dirrec->{volume}): '$path', subdir '$subdir'\n";
		if ($path) { $path .= '/' . $subdir; }
		else { $path = $subdir; }
		if (exists $cajk{$dirrec->{volume}}{$path}) {
			$skip = 1;
			last;
		}
	}
	if ($skip) {
#print STDERR "skip ($dirrec->{volume}): $dirrec->{dir} covered by $path\n";
		next;
	}

	# explore subtree
	$sthsubtreelist->execute($dirrec->{volume}, $dirrec->{dir}."%");
	$new_vortex = 0;
	while (($subrec = $sthsubtreelist->fetchrow_hashref())) {
		if ($subrec->{dir} ne $dirrec->{dir} and $dirrec->{dir} and index($subrec->{dir}, "$dirrec->{dir}/") != 0) {
#print STDERR "filtered out ('$subrec->{dir}' not in '$dirrec->{dir}')\n";
			next;
		}
		$sthdir->execute($dirrec->{volume}, $subrec->{dir}, $login, $dirrec->{rights});
		if (!$sthdir->fetchrow_hashref()) {
# FIXME: new_vortext je třeba i pro každý mointpoint (==> zbytečně procházen findem i každý volume připojený na daném volumu)
			$new_vortex = 1;
			$sthdir->finish;
			last;
		}
		$sthdir->finish;
	}
	$sthsubtreelist->finish;
	if ($new_vortex) {
		$new_vortex = 0;
		$cajk_individual{$dirrec->{volume}}{$dirrec->{dir}} = "$dirrec->{login}	$dirrec->{rights}";
	} else {
	# wow, we got the whole subtree
		$cajk{$dirrec->{volume}}{$dirrec->{dir}} = "$dirrec->{login}	$dirrec->{rights}";
	}
}

$sthseed->finish;

$sthvolumes->execute();
while (($volrec = $sthvolumes->fetchrow_hashref())) {
	$volumes{$volrec->{volume}} = $volrec->{dir};
}
$sthvolumes->finish;

$dbh->disconnect();

if (not defined $servicedir) {
	$servicedir = $default_servicedir;
	print "#service_dir='/afs/...'\n";
}
foreach my $volume (sort keys %cajk_individual) {
	my $mydir;
	my @a;

	if (exists $volumes{$volume}) { $voldir = $volumes{$volume}; }
	else {
		$voldir = "$servicedir";
		print "fs mkmount -dir '$servicedir' -vol $volume\n";
	}
	foreach my $dir (sort keys %{$cajk_individual{$volume}}) {
		if ($dir) { $mydir="/$dir"; }
		else { $mydir=$dir; }
		$mydir = "$voldir$mydir";
		$mydir =~ s/'/\\'/;

		$_ = $cajk_individual{$volume}{$dir};
		@a = split /\s/;
		$login = $a[0];

		print "fs sa '$mydir' $login none\n";
	}
	if (not exists $volumes{$volume}) { print "fs rmmount -dir '$servicedir'\n"; }
}
foreach my $volume (sort keys %cajk) {
	my $mydir;
	my @a;

	if (exists $volumes{$volume}) { $voldir = $volumes{$volume}; }
	else {
		$voldir = "$servicedir";
		print "fs mkmount -dir '$servicedir' -vol $volume\n";
	}
	foreach my $dir (sort keys %{$cajk{$volume}}) {
		if ($dir) { $mydir="/$dir"; }
		else { $mydir=$dir; }
		$mydir = "$voldir$mydir";
		$mydir =~ s/'/\\'/;

		$_ = $cajk{$volume}{$dir};
		@a = split /\s/;
		$login = $a[0];

		print "find '$mydir' -type d -exec fs sa '{}' $login none \\;\n";
	}
	if (not exists $volumes{$volume}) { print "fs rmmount -dir '$servicedir'\n"; }
}
