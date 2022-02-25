CREATE TABLE rights (
	volume VARCHAR(64),
	dir VARBINARY(2048),
	login VARCHAR(64),
	rights VARCHAR(10),

	INDEX(dir),
	INDEX(volume),
	INDEX(login)
) ENGINE='innodb' COLLATE 'utf8_bin';

CREATE TABLE mountpoints (
	pointvolume VARCHAR(64),
	pointdir VARBINARY(2048),
	dir VARBINARY(2048),
	volume VARCHAR(64),

	INDEX(dir),
	INDEX(volume)
) ENGINE='innodb' COLLATE 'utf8_bin';

CREATE TABLE volumes (
	dir VARBINARY(2048),
	volume VARCHAR(64),

	INDEX(dir),
	INDEX(volume)
) ENGINE='innodb' COLLATE 'utf8_bin';
