#!/bin/sh
set -e
ssh -t ulang.io "cd ulang.io/wasm && ./docker/control.sh stop && git pull && ./docker/control.sh start && ./docker/control.sh logs"