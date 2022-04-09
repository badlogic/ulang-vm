#!/bin/bash
if [ -d ./node_modules ]; then
	if ! ./node_modules/esbuild/bin/esbuild --version ; then
		rm -r node_modules
	fi
fi

set -e

if [ -z "$ULANG_DEV" ]; then	
	npm install
	npm rebuild
	npm run build
	node server/build/server.js	
else	
	npm install
	npm rebuild
	npm run dev
fi