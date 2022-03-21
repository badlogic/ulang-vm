import { Axios } from "axios"

const axios = new Axios({
	baseURL: "https://api.github.com/"
})

export interface GistFile {
	filename: string,
	type: string,
	content: string
}

export async function getGist (id: string) {
	let resp = await axios.get(`/gists/${id}`);
	console.log(resp.data);
	if (resp.status != 200) return null;
	return JSON.parse(resp.data) as {
		description: string,
		id: string,
		owner: {
			login: string
		}
		files: {
			"source.ul": GistFile
		}
		comments: number
	};
}