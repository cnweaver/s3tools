#!/bin/sh

CORES=$(grep -c '^processor' /proc/cpuinfo)

cd /s3tools/linux-static-build/cryptopp
echo "Building crypto++"
make -j${CORES} libcryptopp.a
RESULT="$?"
if [ "$RESULT" -ne 0 ]; then
	exit $RESULT
fi

cd ..
if [ ! -e makefile -o  \( ../configure -nt settings.mk \) -o \( ../build-static.sh -nt settings.mk \) ]; then
	echo "Configuring s3tools"
	LDFLAGS="-static -lssl -lcrypto -lnghttp2 -lbrotlidec -lbrotlicommon" ../configure --with-cryptopp-incdir=$(pwd) --with-cryptopp-libdir=$(pwd)/cryptopp
	RESULT="$?"
	if [ "$RESULT" -ne 0 ]; then
		exit $RESULT
	fi
fi

echo "Building s3tools"
make -j${CORES}
RESULT="$?"
if [ "$RESULT" -ne 0 ]; then
	exit $RESULT
fi

for exe in $(ls bin); do
	strip bin/$exe
done
