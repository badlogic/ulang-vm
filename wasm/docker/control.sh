#!/bin/bash

set -e

printHelp () {
	echo "Usage: control.sh <command>"
	echo "Available commands:"
	echo
	echo "   restart    Restarts the Node.js server, pulling in new assets."
	echo "              Uses ULANG_RESTART_PWD env variable."
	echo
	echo "   start      Pulls changes, builds docker image(s), and starts"
	echo "              the services (Nginx, Node.js)."
	echo
	echo "   stop       Stops the services."	
	echo
	echo "   logs       Tail -f services' logs."
	echo
	echo "   shell      Opens a shell into the Node.js container."
}

dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
pushd $dir > /dev/null

case "$1" in
restart)
	curl -X GET https://ulang.io/restart?pwd=$ULANG_RESTART_PWD
	;;
start)
	git pull
	docker-compose build --no-cache
	docker-compose up -d
	;;
stop)
	docker-compose down
	;;
shell)
	docker exec -it ulang_site bash
	;;
logs)
	docker-compose logs -f
	;;
*) 
	echo "Invalid command $1"
	printHelp
	;;
esac

popd > /dev/null