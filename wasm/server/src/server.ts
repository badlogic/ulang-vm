import { createServer, Server } from "http";
import express from "express";
import { setupLiveEdit } from "./liveedit";
import axios from "axios"
import querystring from "query-string"
import Keyv from "keyv"
import fs from "fs"
import path from "path"
import bcrypt from "bcrypt"

const port = process.env.ULANG_PORT || 3000;
const app = express()
const server: Server = createServer(app);
const staticFiles = "../../client/assets";

// Check if all env vars are given
let dataDir = process.env.ULANG_DATA_DIR;
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

if (!dataDir || dataDir.length == 0) {
	console.error("===============================================================================");
	console.error("No ULANG_DATA_DIR found in env.");
	console.error("===============================================================================");
	process.exit(-1);
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
if (dataDir.startsWith(".")) dataDir = path.join(__dirname, dataDir);
if (!fs.existsSync(dataDir))
	fs.mkdirSync(dataDir);
console.log(`Data dir:             ${dataDir}`);

// Setup db
let dbFile = path.join(dataDir, "db.sqlite");
console.log(`DB file:              ${dbFile}`);
const projectsDb = new Keyv(`sqlite://${dbFile}`, { namespace: "projects" });
const hashesDb = new Keyv(`sqlite://${dbFile}`, { namespace: "hashes" });

(projectsDb as any).opts.store.db.pragma("journal_mode = WAL");
(hashesDb as any).opts.store.db.pragma("journal_mode = WAL");

// Setup express 
app.use(express.static("./client/assets"));
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

const saltRounds = 10;

interface Projects {
	user: string,
	projects: { gistId: string, title: string, lastModified: number }[];
}
interface Hashes {
	user: string,
	hashes: string[]
}

// This should be atomic, but it's unlikely a user logs in multiple
// times at the exact same time.
async function createUser (accessToken: string, user: string) {
	let salt = await bcrypt.genSalt(saltRounds);
	let hash = await bcrypt.hash(accessToken, salt);

	let hashes = await hashesDb.get(user);
	if (!hashes) hashes = { user: user, hashes: [] };
	hashes.hashes.push(hash);
	await hashesDb.set(user, hashes);

	let projects = await projectsDb.get(user) as Projects;
	if (!projects) {
		projects = { user: user, projects: [] };
		await projectsDb.set(user, projects);
	}
}

async function isAuthorized (user: string, accessToken: string) {
	let hashes = await hashesDb.get(user) as Hashes;
	let matched = false;
	for (let i = 0; i < hashes.hashes.length; i++) {
		let hash = hashes.hashes[i];
		matched = await bcrypt.compare(accessToken, hash);
		if (matched) break;
	}
	if (!matched) throw new Error("Invalid access token.");
	return true;
}

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

	let projects = await projectsDb.get(user) as Projects;
	let matched = false;
	for (let i = 0; i < projects.projects.length; i++) {
		matched = projects.projects[i].gistId == gistId;
		if (matched) break;
	}
	if (!matched) {
		projects.projects.push({ title: title, gistId: gistId, lastModified: Date.now() });
		await projectsDb.set(user, projects);
	}
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

	let projects = await projectsDb.get(user) as Projects;
	let project = null;
	for (let i = 0; i < projects.projects.length; i++) {
		project = projects.projects[i];
		if (project.gistId == gistId) {
			project.title = title;
			await projectsDb.set(user, projects);
			break;
		}
	}
	if (!project) throw new Error("Project doesn't exist.");
	res.sendStatus(200);
});

app.get("/api/:user/projects", async (req, res) => {
	let user = req.params.user;
	if (!user) throw new Error("No user given.");

	let projects = await projectsDb.get(user);
	if (!projects) throw new Error("User doesn't exist.");
	res.send(projects);
});

// Run server
server.on("listening", () => console.log(`Started on port ${port}`));
server.listen(port);