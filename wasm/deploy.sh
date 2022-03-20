#!/bin/sh
set -e
ssh -t ulang.io "cd ulang.io/wasm &&  sudo ./docker/control.sh stop && sudo git pull && sudo ./docker/control.sh start && sudo ./docker/control.sh logs"