#!/bin/bash
set -e

while :
do
git pull
npm install
npm run build
NODE_DEV=production node server/build/server.js
done