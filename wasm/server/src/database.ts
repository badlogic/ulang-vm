
import { Connection, createPool, Pool, PoolConnection } from "mariadb"
import bcrypt from "bcrypt"
import Knex from "knex";
import path from "path";

const saltRounds = 8;
let pool: Pool;

interface Project {
	user: string,
	title: string,
	gistId: string,
	created?: number,
	modified?: number
}

interface UserHash {
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

	knex.client.pool.destroy();

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
		return await values ? connection.execute(query, values) : connection.query(query);
	} catch (err) {
		throw new Error("Couldn't execute query.");
	} finally {
		if (connection) connection.end();
	}
}

export async function isAuthorized (user: string, accessToken: string) {
	let con: PoolConnection | null = null;
	try {
		con = await pool.getConnection();
		let rows = await con.query("select hash, user from hashes where user=?", [user]) as UserHash[];
		if (rows.length < 1) throw new Error("User not authorized.");

		for (let i = 0; i < rows.length; i++) {
			let hash = rows[i].hash;
			if (await bcrypt.compare(accessToken, hash)) {
				return;
			}
		}

		throw new Error("User not authorized.");
	} finally {
		if (con) con.end();
	}
}

export async function createUser (user: string, accessToken: string) {
	let tokenHash = await hash(accessToken);
	await query("insert into hashes (user, hash) values (?, ?)", [user, tokenHash]);
}

export async function createProject (user: string, title: string, gistId: string) {
	await query("insert into projects (user, title, gistid) values (?, ?, ?)", [user, title, gistId]);
}

export async function updateProject (user: string, title: string, gistId: string) {
	let result = await query("update projects set title=? where user=? and gistid=?", [title, user, gistId]);
	if (result.affectedRows == 0) throw new Error("Couldn't update project.");
}

export async function deleteProject (user: string, gistId: string) {
	let result = await query("delete from projects where user=? and gistid=?", [user, gistId]);
}

export async function getProjects (user: string) {
	let result = await query("select user, title, gistid, UNIX_TIMESTAMP(created) as created, UNIX_TIMESTAMP(modified) as modified from projects where user=? order by modified desc", [user]);
	delete result.meta; // 
	for (let i = 0; i < result.length; i++) {
		result[i].created = parseInt(result[i].created.toString()) * 1000;
		result[i].modified = parseInt(result[i].modified.toString()) * 1000;
	}
	return result as Project[];
}