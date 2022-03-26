import { createServer, Server } from "http";
import express from "express";
import { setupLiveEdit } from "./liveedit";
import axios from "axios"
import querystring from "query-string"
import bcrypt from "bcrypt"
import mariadb from "mariadb"
import { createProject, createUser, getProjects, isAuthorized, setupDb, updateProject } from "./db";

const port = process.env.ULANG_PORT || 3000;
const app = express()
const server: Server = createServer(app);
const staticFiles = "../../client/assets";

// Check if all env vars are given
let clientId = expectEnvVar("ULANG_CLIENT_ID");
let clientSecret = expectEnvVar("ULANG_CLIENT_SECRET");
let dbPassword = expectEnvVar("ULANG_DB_PASSWORD");
let liveEdit = process.env.ULANG_DEV !== undefined;

console.log(`Port:                 ${port}`)
console.log(`GitHub client id:     ${clientId}`);
console.log(`GitHub client secret: ${clientSecret}`);
console.log(`Live edit:            ${liveEdit}`);

// Setup database
setupDb(dbPassword);

// Setup live edit of anything in assets/ when we are not in production mode.
if (liveEdit) setupLiveEdit(server, staticFiles);

// Setup express 
app.use(express.static("./client/assets"));
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

app.post("/api/access_token", async (req, res) => {
	let code = req.body.code as string;
	if (!code) throw new Error("No code given.");
	let resp = await axios.post("https://github.com/login/oauth/access_token", querystring.stringify({
		client_id: clientId,
		client_secret: clientSecret,
		code: code
	}), { headers: { "Accept": "application/json" } });
	if (resp.status >= 400)
		throw new Error(`Couldn't get acess token from GitHub: ${JSON.stringify(resp.data)}`);

	let user = await axios.get("https://api.github.com/user", {
		headers: {
			"Authorization": `token ${resp.data.access_token}`
		}
	});

	await createUser(resp.data.access_token, user.data.login);

	res.send({ accessToken: resp.data.access_token, user: user.data });
});

// This should be atomic, but it's unlikely a user creates a new projects
// on two separate devices at the same time.
app.post("/api/:user/projects", async (req, res) => {
	let user = req.params.user;
	let accessToken = req.body.accessToken;
	let gistId = req.body.gistId;
	let title = req.body.title;
	if (!user) throw new Error("No user given.");
	if (!accessToken) throw new Error("No access token given.");
	if (!gistId) throw new Error("No Gist id given.");
	if (!title) throw new Error("No title given.");

	await isAuthorized(user, accessToken);

	await createProject(user, gistId, title);

	res.sendStatus(200);
});

app.patch("/api/:user/projects", async (req, res) => {
	let user = req.params.user;
	let accessToken = req.body.accessToken;
	let gistId = req.body.gistId;
	let title = req.body.title;
	if (!user) throw new Error("No user given.");
	if (!accessToken) throw new Error("No access token given.");
	if (!gistId) throw new Error("No Gist id given.");
	if (!title) throw new Error("No title given.");

	await isAuthorized(user, accessToken);

	await updateProject(user, gistId, title);

	res.sendStatus(200);
});

app.get("/api/:user/projects", async (req, res) => {
	let user = req.params.user;
	if (!user) throw new Error("No user given.");

	let projects = await getProjects(user);
	if (!projects) throw new Error("User doesn't exist.");
	res.send(projects);
});

// Run server
server.on("listening", () => console.log(`Started on port ${port}`));
server.listen(port);

function expectEnvVar (name: string): string {
	let envVar = process.env[name];
	if (!envVar) {
		console.error("===============================================================================");
		console.error(`No ¢{name} found in env.`);
		console.error("===============================================================================");
		process.exit(-1);
	}
	return envVar;
}