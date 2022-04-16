import { Axios } from "axios"

const axios = new Axios({
	baseURL: "https://api.github.com/"
})

export interface Fork {
	created_at: string,
	updated_at: string,
	id: string,
	url: string,
	user: {
		login: string
	}
}

export interface GistFile {
	filename?: string,
	type?: string,
	content: string,
	deleted?: boolean
}

export type GistFiles = {
	[filename: string]: GistFile
}

export interface Gist {
	description: string,
	id?: string,
	owner?: {
		login: string
	}
	files: GistFiles
	comments?: number
	public?: boolean | string,
	fork_of?: Gist,
	forks?: Fork[]
}

export async function getGist (id: string, accessToken: string) {
	let headers = accessToken ? { headers: { "Authorization": `token ${accessToken}` } } : null;
	let resp = await axios.get(`/gists/${id}?ts=${performance.now()}`, headers);
	// console.log(resp.data);
	if (resp.status != 200) return null;
	return JSON.parse(resp.data) as Gist
}

export async function newGist (title: string, files: GistFiles, accessToken: string) {
	let resp = await axios.post("/gists", JSON.stringify({
		description: title,
		files: files,
		public: true,
	} as Gist), {
		headers: {
			"Accept": "application/vnd.github.v3+json",
			"Authorization": `token ${accessToken}`,
			'Content-Type': 'application/json'
		}
	});
	if (resp.status >= 400)
		throw new Error(`GitHub returned (${resp.status}): ${resp.data}`);
	return JSON.parse(resp.data) as Gist;
}

export async function forkGist (id: string, accessToken: string) {
	let resp = await axios.post(`/gists/${id}/forks`, null, {
		headers: {
			"Accept": "application/vnd.github.v3+json",
			"Authorization": `token ${accessToken}`,
			'Content-Type': 'application/json'
		}
	});
	if (resp.status >= 400)
		throw new Error(`GitHub returned (${resp.status}): ${resp.data}`);
	return JSON.parse(resp.data) as Gist;
}

export async function updateGist (id: string, title: string, files: GistFiles, accessToken: string) {
	let resp = await axios.patch(`/gists/${id}`, JSON.stringify({
		description: title,
		files: files
	} as Gist), {
		headers: {
			"Accept": "application/vnd.github.v3+json",
			"Authorization": `token ${accessToken}`,
			'Content-Type': 'application/json'
		}
	});
	if (resp.status >= 400)
		throw new Error(`GitHub returned (${resp.status}): ${resp.data}`);
	return JSON.parse(resp.data) as Gist;
}