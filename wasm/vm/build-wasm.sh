#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
pushd "$SCRIPT_DIR"
SOURCES=../../src/ulang.c
OPT=-O3

if [[ -z "${EMSDK}" ]]; then
	echo "Please set the environment variable EMSDK to point to your local emscripten directory, i.e. export EMSDK=/path/to/emsdk"
	exit -1;
fi
source "${EMSDK}/emsdk_env.sh"

while getopts ":g" option; do
   case $option in
      g)
        OPT=-g;;
   esac
done

emcc $OPT \
	-s STRICT=1 \
	-s LLD_REPORT_UNDEFINED \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s ALLOW_TABLE_GROWTH \
	-s FILESYSTEM=0 \
	-s MALLOC=emmalloc \
	-s ENVIRONMENT=web \
	-s EXPORTED_FUNCTIONS='["_malloc", "_free"]' \
	-s EXPORTED_RUNTIME_METHODS='["cwrap", "allocateUTF8", "UTF8ArrayToString", "addFunction"]' \
	-s MODULARIZE=1 -s EXPORT_ES6=1 -s USE_ES6_IMPORT_META=0 -s EXPORT_NAME=loadWasm \
	--no-entry \
	-I../../src $SOURCES \
	-o src/ulang.js

mkdir -p dist/iife
cp src/ulang.wasm dist
cp src/ulang.wasm dist/iife

mkdir -p ../client/assets/bundle
cp src/ulang.wasm ../client/assets/bundle

popd