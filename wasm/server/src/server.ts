import { createServer, Server } from "http";
import express, { Response } from "express";
import { setupLiveEdit } from "./liveedit";
import axios from "axios"
import querystring from "query-string"
import { createProject, createUser, getProjects, isAuthorized, setupDb, updateProject } from "./database";
import { body, param, validationResult } from "express-validator"

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

interface ErrorResult {
	message: string,
	detail: string
}

function sendError (res: Response, message: string, detail: string) {
	res.status(400).send({ message: message, detail: detail });
}

app.post("/api/access_token", async (req, res) => {
	try {
		let code = req.body.code as string;
		if (!code) throw new Error("No code given.");
		let params = querystring.stringify({
			client_id: clientId,
			client_secret: clientSecret,
			code: code
		});
		let headers = { headers: { "Accept": "application/json" } };
		let resp = await axios.post("https://github.com/login/oauth/access_token", params, headers);
		if (resp.status >= 400 || resp.data.error)
			throw new Error(`Couldn't get acess token from GitHub: ${JSON.stringify(resp.data)}`);

		let user = await axios.get("https://api.github.com/user", {
			headers: {
				"Authorization": `token ${resp.data.access_token}`
			}
		});

		await createUser(user.data.login, resp.data.access_token);

		res.send({ accessToken: resp.data.access_token, user: user.data });
	} catch (err) {
		sendError(res, "Couldn't create user.", JSON.stringify(err, Object.getOwnPropertyNames(err)));
	}
});

app.post("/api/:user/projects",
	param("user").exists().trim().notEmpty().escape(),
	body("accessToken").exists().trim().notEmpty(),
	body("gistId").exists().trim().notEmpty(),
	body("title").exists().trim().notEmpty().escape(),
	async (req, res) => {
		try {
			if (!validationResult(req).isEmpty()) throw new Error("Invalid inputs");

			let user = req.params?.user;
			let accessToken = req.body.accessToken;
			let gistId = req.body.gistId;
			let title = req.body.title;

			await isAuthorized(user, accessToken);

			await createProject(user, title, gistId);

			res.sendStatus(200);
		} catch (err) {
			sendError(res, "Couldn't create project.", JSON.stringify(err, Object.getOwnPropertyNames(err)));
		}
	});

app.patch("/api/:user/projects",
	param("user").exists().trim().notEmpty().escape(),
	body("accessToken").exists().trim().notEmpty(),
	body("gistId").exists().trim().notEmpty(),
	body("title").exists().trim().notEmpty().escape(),
	async (req, res) => {
		try {
			if (!validationResult(req).isEmpty()) throw new Error("Invalid inputs");

			let user = req.params?.user;
			let accessToken = req.body.accessToken;
			let gistId = req.body.gistId;
			let title = req.body.title;

			await isAuthorized(user, accessToken);

			await updateProject(user, title, gistId);

			res.sendStatus(200);
		} catch (err) {
			sendError(res, "Couldn't update project.", JSON.stringify(err, Object.getOwnPropertyNames(err)));
		}
	});

app.get("/api/:user/projects",
	param("user").exists().trim().notEmpty(),
	async (req, res) => {
		try {
			if (!validationResult(req).isEmpty()) throw new Error("Invalid inputs");

			let user = req.params?.user;
			if (!user) throw new Error("No user given.");

			let projects = await getProjects(user);
			if (!projects) throw new Error("User doesn't exist.");
			res.send(projects);
		} catch (err) {
			sendError(res, "Couldn't get user projects.", JSON.stringify(err, Object.getOwnPropertyNames(err)));
		}
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