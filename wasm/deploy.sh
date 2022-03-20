#!/bin/sh
set -e
if [ -z "ULANG_CLIENT_SECRET" ]; then
	echo "Set ULANG_CLIENT_SECRET from the GitHub OAuth app settings."
	exit 1
fi
ssh -t ulang.io "cd ulang.io/wasm && ./docker/control.sh stop && ULANG_CLIENT_SECRET=${ULANG_CLIENT_SECRET} git pull && ./docker/control.sh start && ./docker/control.sh logs"