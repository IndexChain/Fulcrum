#!/bin/bash

# This runs inside the Docker image

set -e  # Exit on error

if [ -z "$1" ] || [ -z "$2" ]; then
    echo "Please pass Fulcrum and rocksdb dirnames as the two args"
    exit 1
fi

PACKAGE="$1"
ROCKSDB_PACKAGE="$2"
TARGET_BINARY=Fulcrum

top=/work
cd "$top" || fail "Could not cd $top"
. "$top/$PACKAGE/contrib/build/common/common.sh" || (echo "Cannot source common.h" && exit 1)

info "Building RocksDB ..."
cd "$top/$ROCKSDB_PACKAGE" || fail "Could not cd tp $ROCKSDB_PACKAGE"
USE_RTTI=1 PORTABLE=1 make static_lib -j`nproc` V=1 || fail "Could not build RocksDB"

info "Stripping librocksdb.a ..."
strip -g librocksdb.a || fail "Could not strip librocksdb.a"

info "Copying librocksdb.a to Fulcrum directory ..."
cp -fpva librocksdb.a "$top"/"$PACKAGE"/staticlibs/rocksdb/bin/linux || fail "Could not copy librocksdb.a"
printok "RocksDB built and moved to Fulcrum staticlibs directory"

cd "$top"/"$PACKAGE" || fail "Could not chdir to Fulcrum dir"

info "Building Fulcrum ..."
qmake || fail "Could not run qmake"
make -j`nproc` || fail "Could not run make"

ls -al "$TARGET_BINARY" || fail "$TARGET_BINARY not found"
printok "$TARGET_BINARY built"

info "Copying to top level ..."
mkdir -p "$top/built" || fail "Could not create build products directory"
cp -fpva "$TARGET_BINARY" "$top/built/." || fail "Could not copy $TARGET_BINARY"
cd "$top" || fail "Could not cd to $top"

printok "Inner _build.sh finished"
