## tinyafs

AFS scanner library and utilities

### Installation

#### Dependencies

For *tinyafs*:

* libopenafs-dev / openafs-devel
* libglite-lbjp-common-db-dev / glite-lbjp-common-db-devel

For *glite-lbjp-common-db*:

* libmysqlclient-dev / mariadb-devel
* liblog4c-dev / log4c-devel

#### Build glite-lbjp-common-db

    git clone https://github.com/CESNET/glite-lb
    cd glite-lb
    org .glite.lb/configure --prefix=/usr/local --root=/ --enable-db --with-postgresql=no

    # will do also installation
    su
      make

#### Build tinyafs

    cat > Makefile.inc <<EOF
    prefix=/usr/local
    CPPFLAGS=-I/usr/local/include
    LDFLAGS=-I/usr/local/lib64
    EOF
    make

    su
      make install
