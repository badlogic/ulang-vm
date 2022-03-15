#!/bin/bash

set -e

printHelp () {
	echo "Usage: control.sh <command>"
	echo "Available commands:"
	echo
	echo "   restart      Restarts the Node.js server, pulling in new assets."
	echo "                Uses ULANG_RESTART_PWD env variable."
	echo
	echo "   start        Pulls changes, builds docker image(s), and starts"
	echo "                the services (Nginx, Node.js)."
	echo "   startdev     Pulls changes, builds docker image(s), and starts"
	echo "                the services (Nginx, Node.js)."
	echo
	echo "   reloadnginx  Reloads the nginx configuration"
	echo
	echo "   stop         Stops the services."	
	echo
	echo "   logs         Tail -f services' logs."
	echo
	echo "   shell        Opens a shell into the Node.js container."
	echo
	echo "   shellnginx   Opens a shell into the Nginx container."
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
startdev)
	git pull
	docker-compose -f docker-compose.yml -f docker-compose.dev.yml build
	docker-compose -f docker-compose.yml -f docker-compose.dev.yml up -d
	;;
reloadnginx)
	docker exec -it ulang_nginx nginx -t
	docker exec -it ulang_nginx nginx -s reload
	;;
stop)
	docker-compose down -t 1
	;;
shell)
	docker exec -it ulang_site bash
	;;
shellnginx)
	docker exec -it ulang_nginx bash
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