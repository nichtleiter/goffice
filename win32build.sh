#!/usr/bin/bash

source /etc/makepkg_mingw64.conf

COMMON_OPTS="--host=${MINGW_CHOST} \
--target=${MINGW_CHOST} \
--enable-compile-warnings=no \
--build=${MINGW_CHOST} \
--prefix=${MINGW_PREFIX} \
--libexecdir=${MINGW_PREFIX}/lib \
--with-config-backend=win32 \
--enable-shared=no \
--without-long-double \
--enable-introspection=no"

export ACLOCAL_PATH=/mingw64/share/aclocal/ 

./autogen.sh \
$COMMON_OPTS

make
