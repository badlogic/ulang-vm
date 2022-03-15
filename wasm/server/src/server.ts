import { createServer } from "http";
import express from "express";
import { setupLiveEdit } from "./liveedit";

const port = 3000;
const restartPassword = process.env.RESTART_PWD || "1234";
const app = express()
const server = createServer(app);
const staticFiles = "../../client/assets";

// Setup live edit of anything in assets/
// when we are not in production mode.
if (process.env.NODE_DEV != "production") {
	setupLiveEdit(app, staticFiles);
}

// Setup express
app.use(express.static("./client/assets"));
app.get("/restart", (req, res) => {
	if (restartPassword === req.query["pwd"]) {
		res.sendStatus(200);
		setTimeout(() => process.exit(0), 1000);
	} else res.sendStatus(500);
});

// Run server
server.on("listening", () => console.log(`YOLO on port ${port}`));
server.listen(port);