#!/bin/sh
set -e
if [ -z "ULANG_CLIENT_SECRET" ]; then
	echo "Set ULANG_CLIENT_SECRET from the GitHub OAuth app settings."
	exit 1
fi
CMD="cd ulang.io/wasm && ./docker/control.sh stop && git pull && ULANG_CLIENT_SECRET=${ULANG_CLIENT_SECRET} ./docker/control.sh start && ./docker/control.sh logs"
echo $CMD
ssh -t ulang.io $CMD