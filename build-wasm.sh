#!/bin/bash
SOURCES=src/ulang.c
OPT=-O3

while getopts ":g" option; do
   case $option in
      g)
        OPT=-g;;
   esac
done

emcc $OPT \
	-s WASM=1 \
	-s LLD_REPORT_UNDEFINED \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s ALLOW_TABLE_GROWTH \
	-s EXPORTED_FUNCTIONS='["_malloc", "_free"]' \
	-s EXPORTED_RUNTIME_METHODS='["cwrap", "allocateUTF8", "UTF8ArrayToString", "addFunction"]' \
	-s MODULARIZE=1 -s 'EXPORT_NAME="ulang"' \
	--pre-js src/wasm/pre.js \
	--no-entry \
	-Isrc $SOURCES \
	-o src/wasm/ulang.js

ls -lah src/wasm
