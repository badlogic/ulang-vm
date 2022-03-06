#!/bin/sh
set -e
npm run clean
npm run build:wasm
npm run build:ulang-vm
npm run build:ulang-site
npm run minify

rsync -av ulang-site/assets/ marioslab.io:/home/badlogic/marioslab.io/data/web/ulang