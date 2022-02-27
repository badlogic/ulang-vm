#!/bin/bash
SOURCES=src/ulang.c
emcc -g \
	-s WASM=1 \
	-s LLD_REPORT_UNDEFINED \
	-s ALLOW_MEMORY_GROWTH=1 \
	-s EXPORTED_FUNCTIONS='["_malloc", "_free"]' \
	-s EXPORTED_RUNTIME_METHODS='["cwrap", "allocateUTF8", "UTF8ArrayToString"]' \
	-s MODULARIZE=1 -s 'EXPORT_NAME="ulang"' \
	--pre-js src/wasm/pre.js \
	--no-entry \
	-Isrc $SOURCES \
	-o src/wasm/ulang.js

ls -lah src/wasm
