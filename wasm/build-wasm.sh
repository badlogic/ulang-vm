#!/bin/bash
SOURCES=../src/ulang.c
OPT=-O3

while getopts ":g" option; do
   case $option in
      g)
        OPT=-g;;
   esac
done

mkdir -p ulang-vm/dist

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
	-s MODULARIZE=1 -s EXPORT_ES6=1 -s USE_ES6_IMPORT_META=0 -s 'EXPORT_NAME="ulang"' \
	--pre-js ulang-vm/src/pre.js \
	--extern-post-js ulang-vm/src/post.js \
	--no-entry \
	-I../src $SOURCES \
	-o ulang-vm/src/ulang.js

ls -lah ulang-vm/dist
