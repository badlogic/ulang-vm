import { Socket, io } from "socket.io-client"

export async function setupLiveEdit () {
	let lastChangeTimestamp = null;
	this.socket = io({ path: "/ws", transports: ['websocket'] });
	this.socket.on("connect", () => console.log("Connected"));
	this.socket.on("disconnect", () => console.log("Disconnected"));
	this.socket.on("message", (timestamp) => {
		if (lastChangeTimestamp != timestamp) {
			setTimeout(() => location.reload(), 100);
			lastChangeTimestamp = timestamp;
		}
	});
};