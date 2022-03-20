#!/bin/bash
set -e

if [ -d ./node_modules ]; then
	if ! ./node_modules/esbuild/bin/esbuild --version ; then
		rm -r node_modules
	fi
fi

echo "DEV: ${ULANG_DEV}"

if [[ -z "${ULANG_DEV}" ]]; then
	echo "=================== PROD MODE ==================="
	npm install
	npm run build
	NODE_DEV=production node server/build/server.js	
else
	echo "=================== DEV MODE ==================="
	npm install
	npm run dev
fi