import { createServer, Server } from "http";
import express from "express";
import { setupLiveEdit } from "./liveedit";
import axios from "axios"
import querystring from "query-string"

const port = process.env.ULANG_PORT || 3000;
const app = express()
const server: Server = createServer(app);
const staticFiles = "../../client/assets";

// Check if ULANG_CLIENT_SECRET is given
let clientId = process.env.ULANG_CLIENT_ID;
let clientSecret = process.env.ULANG_CLIENT_SECRET;
// Setup live edit of anything in assets/
// when we are not in production mode.
if (process.env.ULANG_DEV) {
	clientId = process.env.ULANG_CLIENT_ID_DEV;
	clientSecret = process.env.ULANG_CLIENT_SECRET_DEV;
	setupLiveEdit(server, staticFiles);
	console.log("Live-edit:            true");
} else {
	console.log("Live-edit:            false");
}

if (!clientSecret || clientSecret.length == 0) {
	console.error("===============================================================================");
	console.error("No ULANG_CLIENT_SECRET found in env. Set it from the GitHub OAuth app settings.");
	console.error("===============================================================================");
	process.exit(-1);
}
if (!clientId || clientId.length == 0) {
	console.error("===============================================================================");
	console.error("No ULANG_CLIENT_ID found in env. Set it from the GitHub OAuth app settings.");
	console.error("===============================================================================");
	process.exit(-1);
}

console.log(`Port:                 ${port}`)
console.log(`GitHub client id:     ${clientId}`);
console.log(`GitHub client secret: ${clientSecret}`);

// Setup express
app.use(express.static("./client/assets"));
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

app.post("/api/access_token", async (req, res) => {
	let code = req.body.code as string;
	let accessToken = await axios.post("https://github.com/login/oauth/access_token", querystring.stringify({
		client_id: clientId,
		client_secret: clientSecret,
		code: code
	}), { headers: { "Accept": "application/json" } });

	let user = await axios.get("https://api.github.com/user", {
		headers: {
			"Authorization": `token ${accessToken.data.access_token}`
		}
	});
	res.send({ accessToken: code, user: user.data });
});

// Run server
server.on("listening", () => console.log(`Started on port ${port}`));
server.listen(port);