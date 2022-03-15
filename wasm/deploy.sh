#!/bin/sh
set -e
npm run clean
npm run build:wasm
npm run build:vm
npm run build:client
npm run build:server
npm run minify

rsync -av ulang-site/assets/ marioslab.io:/home/badlogic/marioslab.io/data/web/ulang