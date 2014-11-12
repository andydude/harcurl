#!/bin/bash

ACLOCAL=aclocal
AUTOCONF=autoconf
AUTOMAKE=automake
AUTORECONF=autoreconf

if [ "$(uname -s)" = "Darwin" ]; then
    LIBTOOL=glibtool
    LIBTOOLIZE=glibtoolize
else
    LIBTOOL=libtool
    LIBTOOLIZE=libtoolize
fi

mkdir -p aclocal.d
mkdir -p config.aux

$ACLOCAL

$LIBTOOLIZE

$AUTOMAKE --add-missing

$AUTORECONF
