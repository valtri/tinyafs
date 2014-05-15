CREATE TABLE rights (
	volume VARCHAR(64),
	dir VARCHAR(256),
	login VARCHAR(32),
	rights VARCHAR(10),

	INDEX(dir),
	INDEX(volume),
	INDEX(login)
) ENGINE='innodb' COLLATE 'utf8_bin';

CREATE TABLE mountpoints (
	pointvolume VARCHAR(64),
	pointdir VARCHAR(256),
	dir VARCHAR(256),
	volume VARCHAR(64),

	INDEX(dir),
	INDEX(volume)
) ENGINE='innodb' COLLATE 'utf8_bin';

CREATE TABLE volumes (
	dir VARCHAR(256),
	volume VARCHAR(64),

	INDEX(dir),
	INDEX(volume)
) ENGINE='innodb' COLLATE 'utf8_bin';
