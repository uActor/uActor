#!/bin/bash
set -e

HOST_OS=$(uname)
TAG=$1

# https://v8.dev/docs/embed
build() {
tools/dev/v8gen.py gen $1.release.sample
gn gen out.gn/$1.release.sample/
ninja -C out.gn/$1.release.sample/ v8_monolith
cp out.gn/$1.release.sample/obj/libv8_monolith.a ../../precompiled/${TAG}/linux_$1_libv8_monolith.a 
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

build x64