#!/bin/bash
set -e
if [ -z "$1" ]; then
    echo "Usage: $0 <installation-directory>"
    exit 1
fi
DSTDIR=$1
mkdir -p "$DSTDIR"
SRCDIR=$(pwd)
# Arşivi aç
tar xzf bochs-2.6.11.tar.gz
cd bochs-2.6.11
# Yamaları uygula (Eğer yamalar yoksa hata vermemesi için kontrol ekliyoruz)
for patch in ../bochs-2.6.11-*.patch; do
    if [ -f "$patch" ]; then patch -p1 < "$patch"; fi
done
# Bochs (GDB Stub) Derle
./configure --enable-gdb-stub --with-x --with-x11 --with-term --with-nogui --prefix="$DSTDIR"
make
make install
# Bochs-dbg (Debugger) Derle
make clean
./configure --enable-debugger --with-x --with-x11 --with-term --with-nogui --prefix="$DSTDIR"
make
mv bochs "$DSTDIR/bin/bochs-dbg"
echo "Kurulum tamamlandı: $DSTDIR/bin"
