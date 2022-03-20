import { Express, Response } from "express";
import * as path from "path";
import chokidar from "chokidar"
import { Server } from "http";
import { Server as SocketServer, Socket } from "socket.io"

var lastChangeTimestamp = 0;

export function setupLiveEdit (server: Server, assetPath: string) {
	const sockets: Socket[] = [];
	const io = new SocketServer(server, { path: "/ws" });
	io.on('connection', (socket) => sockets.push(socket));

	let p = path.join(__dirname, assetPath);
	chokidar.watch(p).on('all', () => {
		lastChangeTimestamp = Date.now()
		for (let i = 0; i < sockets.length; i++) {
			sockets[i].send(`${lastChangeTimestamp}`);
		}
	});
}