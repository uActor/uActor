#!/bin/bash
set -e

HOST_OS=$(uname)
HOST_ARCH=$(uname -m)
TAG=$1

# https://v8.dev/docs/embed
build() {
tools/dev/v8gen.py gen $1.release
cp ../../build_args/args.gn.$1 out.gn/$1.release/args.gn
gn gen out.gn/$1.release/
ninja -j 20 -C out.gn/$1.release/ v8_monolith
cp out.gn/$1.release/obj/libv8_monolith.a ../../precompiled/${TAG}/linux_$1_libv8_monolith.a 
}

mkdir -p precompiled/${TAG}
mkdir -p ${HOST_OS}
cd ${HOST_OS}
if [ ! -d "v8" ]; then
fetch v8
fi
cd v8
git checkout refs/tags/${TAG}
gclient sync

vpython build/linux/sysroot_scripts/install-sysroot.py --arch=x64
vpython build/linux/sysroot_scripts/install-sysroot.py --arch=arm
vpython build/linux/sysroot_scripts/install-sysroot.py --arch=arm64

build arm
build x64
build arm64