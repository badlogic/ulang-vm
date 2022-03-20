import { createServer, Server } from "http";
import express from "express";
import { setupLiveEdit } from "./liveedit";

const port = 3000;
const restartPassword = process.env.RESTART_PWD || "1234";
const app = express()
const server: Server = createServer(app);
const staticFiles = "../../client/assets";

// Setup live edit of anything in assets/
// when we are not in production mode.
if (process.env.NODE_DEV != "production") {
	console.log("Setting up live-edit.")
	setupLiveEdit(server, staticFiles);
}

// Setup express
app.use(express.static("./client/assets"));

// Run server
server.on("listening", () => console.log(`YOLO on port ${port}`));
server.listen(port);