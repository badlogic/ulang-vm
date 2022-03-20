import { createServer, Server } from "http";
import express from "express";
import { setupLiveEdit } from "./liveedit";

const port = process.env.ULANG_PORT || 3000;
const app = express()
const server: Server = createServer(app);
const staticFiles = "../../client/assets";

// Check if ULANG_CLIENT_SECRET is given
const clientId = process.env.ULANG_CLIENT_ID || "98d7ee42b2a5f52f8e97";
const clientSecret = process.env.ULANG_CLIENT_SECRET;
if (!clientSecret || clientSecret.length == 0) {
	console.log("No ULANG_CLIENT_SECRET found in env. Set it from the GitHub OAuth app settings.")
	process.exit(-1);
}

console.log(`Port:                 ${port}`)
console.log(`GitHub client id:     ${clientId}`);
console.log(`GitHub client secret: ${clientSecret}`);

// Setup live edit of anything in assets/
// when we are not in production mode.
if (process.env.ULANG_DEV) {
	setupLiveEdit(server, staticFiles);
	console.log("Live-edit:            true");
} else {
	console.log("Live-edit:            false");
}

// Setup express
app.use(express.static("./client/assets"));

// Run server
server.on("listening", () => console.log(`Started on port ${port}`));
server.listen(port);