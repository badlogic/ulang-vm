
import { Connection, createPool, Pool, PoolConnection } from "mariadb"
import bcrypt from "bcrypt"
import Knex from "knex";
import path from "path";

const saltRounds = 10;
let pool: Pool;

interface Project {
	user: string,
	title: string,
	gistId: string,
}

interface User {
	hash: string,
	user: string
}

export async function setupDb (rootPassword: string) {
	const knex = Knex({
		client: "mysql",
		connection: {
			host: 'database',
			user: 'root',
			database: "ulang",
			password: rootPassword,
		},
	});

	await knex.migrate.latest({
		directory: path.join(__dirname, "../schema")
	});

	knex.client.pool.destroy()

	pool = createPool({
		host: 'database',
		user: 'root',
		database: "ulang",
		password: rootPassword,
		connectionLimit: 30
	});
}

async function hash (value: string) {
	let salt = await bcrypt.genSalt(saltRounds);
	return await bcrypt.hash(value, salt);
}

async function checkHash (value: string, hash: string) {
	return await bcrypt.compare(value, hash);
}

async function query (query: string, values?: any[]) {
	let connection: PoolConnection | null = null;
	try {
		connection = await pool.getConnection();
		return await values ? connection.query(query, values) : connection.query(query);
	} catch (err) {
		throw new Error("Couldn't execute query.");
	} finally {
		if (connection) connection.end();
	}
}

export async function isAuthorized (user: string, accessToken: string) {
	let con = await pool.getConnection();
	let rows = await con.query("select hash, user from hashes where user = ?", [user]);
}

export async function createUser (user: string, accessToken: string) {
	let tokenHash = hash(accessToken);

}

export async function createProject (user: string, gistId: string, title: string) {

}

export async function updateProject (user: string, gistId: string, title: string) {

}

export async function getProjects (user: string) {
	return [] as Project[];
}